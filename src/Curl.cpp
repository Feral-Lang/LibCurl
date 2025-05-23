#include "Curl.hpp"

namespace fer
{

constexpr size_t CURL_DEFAULT_PROGRESS_INTERVAL_TICK_MAX = 10;

void setEnumVars(VirtualMachine &vm, VarModule *mod, ModuleLoc loc);

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Callbacks ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

int curlProgressCallback(void *ptr, curl_off_t dlTotal, curl_off_t dlDone, curl_off_t ulTotal,
			 curl_off_t ulDone)
{
	// ensure that the file to be downloaded is not empty
	// because that would cause a division by zero error later on
	if(dlTotal <= 0 && ulTotal <= 0) return 0;

	CurlCallbackData &cbdata = *(CurlCallbackData *)ptr;

	if(!cbdata.curl->getProgressCB()) return 0;

	size_t &intervalTick = cbdata.curl->getProgIntervalTick();
	if(intervalTick < cbdata.curl->getProgIntervalTickMax()) {
		++intervalTick;
		return 0;
	}
	intervalTick = 0;

	VarVec *argsVar = cbdata.curl->getProgressCBArgs();
	as<VarFlt>(argsVar->at(1))->setVal(dlTotal);
	as<VarFlt>(argsVar->at(2))->setVal(dlDone);
	as<VarFlt>(argsVar->at(3))->setVal(ulTotal);
	as<VarFlt>(argsVar->at(4))->setVal(ulDone);
	if(!cbdata.curl->getProgressCB()->call(cbdata.vm, cbdata.loc, argsVar->getVal(), {})) {
		cbdata.vm.warn(cbdata.loc, "failed to call progress callback, check error above");
		return 1;
	}
	return 0;
}

size_t curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	CurlCallbackData &cbdata = *(CurlCallbackData *)userdata;
	if(!cbdata.curl->getWriteCB()) return size * nmemb; // returning zero is an error

	VarVec *argsVar = cbdata.curl->getWriteCBArgs();
	as<VarStr>(argsVar->at(1))->setVal(StringRef(ptr, size * nmemb));
	if(!cbdata.curl->getWriteCB()->call(cbdata.vm, cbdata.loc, argsVar->getVal(), {})) {
		cbdata.vm.warn(cbdata.loc, "failed to call write callback, check error above");
		return 0;
	}
	return size * nmemb;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// VarCurl //////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurl::VarCurl(ModuleLoc loc, CURL *val)
	: Var(loc, false, false), val(val), progCB(nullptr), writeCB(nullptr), progCBArgs(nullptr),
	  writeCBArgs(nullptr), progIntervalTick(0),
	  progIntervalTickMax(CURL_DEFAULT_PROGRESS_INTERVAL_TICK_MAX)
{}
VarCurl::~VarCurl()
{
	clearMimeData();
	curl_easy_cleanup(val);
}

void VarCurl::onCreate(MemoryManager &mem)
{
	progCBArgs = Var::makeVarWithRef<VarVec>(mem, getLoc(), 5, true);
	progCBArgs->push(nullptr);
	progCBArgs->push(Var::makeVarWithRef<VarFlt>(mem, getLoc(), 0.0));
	progCBArgs->push(Var::makeVarWithRef<VarFlt>(mem, getLoc(), 0.0));
	progCBArgs->push(Var::makeVarWithRef<VarFlt>(mem, getLoc(), 0.0));
	progCBArgs->push(Var::makeVarWithRef<VarFlt>(mem, getLoc(), 0.0));

	writeCBArgs = Var::makeVarWithRef<VarVec>(mem, ModuleLoc(), 2, true);
	writeCBArgs->push(nullptr);
	writeCBArgs->push(Var::makeVarWithRef<VarStr>(mem, getLoc(), ""));
}
void VarCurl::onDestroy(MemoryManager &mem)
{
	Var::decVarRef(mem, writeCBArgs);
	Var::decVarRef(mem, progCBArgs);
	setProgressCB(mem, nullptr, {});
	setWriteCB(mem, nullptr, {});
}

void VarCurl::setProgressCB(MemoryManager &mem, VarFn *_progCB, Span<Var *> args)
{
	if(progCB) Var::decVarRef(mem, progCB);
	progCB = _progCB;
	if(progCB) Var::incVarRef(progCB);
	if(!progCBArgs) return;
	while(progCBArgs->size() > 5) {
		Var::decVarRef(mem, progCBArgs->back());
		progCBArgs->pop();
	}
	for(auto &arg : args) {
		Var::incVarRef(arg);
		progCBArgs->push(arg);
	}
}
void VarCurl::setWriteCB(MemoryManager &mem, VarFn *_writeCB, Span<Var *> args)
{
	if(writeCB) Var::decVarRef(mem, writeCB);
	writeCB = _writeCB;
	if(writeCB) Var::incVarRef(writeCB);
	if(!writeCBArgs) return;
	while(writeCBArgs->size() > 5) {
		Var::decVarRef(mem, writeCBArgs->back());
		writeCBArgs->pop();
	}
	for(auto &arg : args) {
		Var::incVarRef(arg);
		writeCBArgs->push(arg);
	}
}

curl_mime *VarCurl::createMime(VirtualMachine &vm, ModuleLoc loc, Var *data)
{
	if(data->is<VarMap>() && as<VarMap>(data)->getVal().empty()) return nullptr;

	mimelist.push_front(curl_mime_init(val));
	curl_mime *mime = mimelist.front();
	if(data->is<VarStr>()) {
		curl_mimepart *part = curl_mime_addpart(mime);
		curl_mime_filedata(part, as<VarStr>(data)->getVal().c_str());
		curl_mime_filename(part, as<VarStr>(data)->getVal().c_str());
	} else {
		VarMap *map = as<VarMap>(data);
		for(auto &item : map->getVal()) {
			Var *v = nullptr;
			Array<Var *, 1> tmp{item.second};
			if(!vm.callVarAndExpect<VarStr>(loc, "str", v, tmp, {})) {
				curl_mime_free(mime);
				mimelist.pop_front();
				return nullptr;
			}
			const String &str   = as<VarStr>(v)->getVal();
			curl_mimepart *part = curl_mime_addpart(mime);
			curl_mime_name(part, item.first.c_str());
			curl_mime_data(part, str.c_str(), str.size());
			vm.decVarRef(v);
		}
	}
	return mime;
}
void VarCurl::clearMimeData()
{
	while(!mimelist.empty()) {
		curl_mime_free(mimelist.front());
		mimelist.pop_front();
	}
}

curl_slist *VarCurl::createSList(VirtualMachine &vm, ModuleLoc loc, Var *data)
{
	if(data->is<VarMap>() && as<VarMap>(data)->getVal().empty()) return nullptr;

	sllist.push_front(nullptr);
	curl_slist *&lst = sllist.front();
	if(data->is<VarStr>()) {
		lst = curl_slist_append(lst, as<VarStr>(data)->getVal().c_str());
	} else {
		String tmpStr;
		VarMap *map = as<VarMap>(data);
		for(auto &item : map->getVal()) {
			Var *v = nullptr;
			Array<Var *, 1> tmp{item.second};
			if(!vm.callVarAndExpect<VarStr>(loc, "str", v, tmp, {})) {
				curl_slist_free_all(lst);
				sllist.pop_front();
				return nullptr;
			}
			const String &str = as<VarStr>(v)->getVal();
			tmpStr.clear();
			tmpStr += item.first;
			tmpStr += ": ";
			tmpStr += str.c_str();
			lst = curl_slist_append(lst, tmpStr.c_str());
			vm.decVarRef(v);
		}
	}
	return lst;
}
void VarCurl::clearSList()
{
	while(!sllist.empty()) {
		curl_slist_free_all(sllist.front());
		sllist.pop_front();
	}
}

CurlCallbackData::CurlCallbackData(ModuleLoc loc, VirtualMachine &vm, VarCurl *curl)
	: loc(loc), vm(vm), curl(curl)
{}

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Functions ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

Var *feralCurlGlobalTrace(VirtualMachine &vm, ModuleLoc loc, Span<Var *> args,
			  const StringMap<AssnArgData> &assn_args)
{
	if(!args[1]->is<VarStr>()) {
		vm.fail(args[1]->getLoc(), "expected trace config to be of type 'str', found: ",
			vm.getTypeName(args[1]));
		return nullptr;
	}
	int res = curl_global_trace(as<VarStr>(args[1])->getVal().c_str());
	return vm.makeVar<VarInt>(loc, res);
}

Var *feralCurlEasyInit(VirtualMachine &vm, ModuleLoc loc, Span<Var *> args,
		       const StringMap<AssnArgData> &assn_args)
{
	CURL *curl = curl_easy_init();
	if(!curl) {
		vm.fail(loc, "failed to run curl_easy_init()");
		return nullptr;
	}
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgressCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
	return vm.makeVar<VarCurl>(loc, curl);
}

Var *feralCurlEasyPerform(VirtualMachine &vm, ModuleLoc loc, Span<Var *> args,
			  const StringMap<AssnArgData> &assn_args)
{
	CURL *curl = as<VarCurl>(args[0])->getVal();
	CurlCallbackData cbdata(loc, vm, as<VarCurl>(args[0]));
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cbdata);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cbdata);
	return vm.makeVar<VarInt>(loc, curl_easy_perform(curl));
}

Var *feralCurlEasyStrErrFromInt(VirtualMachine &vm, ModuleLoc loc, Span<Var *> args,
				const StringMap<AssnArgData> &assn_args)
{
	if(!args[1]->is<VarInt>()) {
		vm.fail(args[1]->getLoc(), "expected error code to be of type 'int', found: ",
			vm.getTypeName(args[1]));
		return nullptr;
	}
	CURLcode code = (CURLcode)as<VarInt>(args[1])->getVal();
	return vm.makeVar<VarStr>(loc, curl_easy_strerror(code));
}

Var *feralCurlSetProgressCBTick(VirtualMachine &vm, ModuleLoc loc, Span<Var *> args,
				const StringMap<AssnArgData> &assn_args)
{
	Var *arg = args[1];
	if(!arg->is<VarInt>()) {
		vm.fail(loc,
			"expected an integer as parameter for setting progress callback"
			"tick interval, found: ",
			vm.getTypeName(arg));
		return nullptr;
	}
	VarCurl *curl = as<VarCurl>(args[0]);
	curl->setProgIntervalTickMax(as<VarInt>(arg)->getVal());
	return vm.getNil();
}

Var *feralCurlEasyGetInfoNative(VirtualMachine &vm, ModuleLoc loc, Span<Var *> args,
				const StringMap<AssnArgData> &assn_args)
{
	CURL *curl = as<VarCurl>(args[0])->getVal();
	if(!args[1]->is<VarInt>()) {
		vm.fail(loc, "expected an integer as parameter for option type, found: ",
			vm.getTypeName(args[1]));
		return nullptr;
	}
	int opt	 = as<VarInt>(args[1])->getVal();
	Var *arg = args[2];

	int res = CURLE_OK;
	// manually handle each of the options and work accordingly
	switch(opt) {
	case CURLINFO_ACTIVESOCKET: {
		if(!arg->is<VarInt>()) {
			vm.fail(loc, "expected an integer as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		long sockfd;
		res = curl_easy_getinfo(curl, (CURLINFO)opt, &sockfd);
		as<VarInt>(arg)->setVal(sockfd);
		break;
	}
	default: {
		vm.fail(loc, "operation is not yet implemented");
		return nullptr;
	}
	}
	return vm.makeVar<VarInt>(loc, res);
}

Var *feralCurlEasySetOptNative(VirtualMachine &vm, ModuleLoc loc, Span<Var *> args,
			       const StringMap<AssnArgData> &assn_args)
{
	VarCurl *varCurl = as<VarCurl>(args[0]);
	CURL *curl	 = varCurl->getVal();
	if(!args[1]->is<VarInt>()) {
		vm.fail(loc,
			"expected an integer (CURL_OPT_*) as parameter for option type, found: ",
			vm.getTypeName(args[1]));
		return nullptr;
	}
	int opt	 = as<VarInt>(args[1])->getVal();
	Var *arg = args[2];

	int res = CURLE_OK;
	// manually handle each of the options and work accordingly
	switch(opt) {
	case CURLOPT_CONNECT_ONLY:   // fallthrough
	case CURLOPT_FOLLOWLOCATION: // fallthrough
	case CURLOPT_NOPROGRESS:     // fallthrough
	case CURLOPT_VERBOSE: {
		if(!arg->is<VarInt>()) {
			vm.fail(loc, "expected an integer as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		res = curl_easy_setopt(curl, (CURLoption)opt, as<VarInt>(arg)->getVal());
		break;
	}
	case CURLOPT_POSTFIELDS: {
		// We don't want POSTFIELDS as it doesn't copy the string data to curl,
		// which is annoying to deal with.
		opt = CURLOPT_COPYPOSTFIELDS;
	}
	case CURLOPT_URL:
	case CURLOPT_USERAGENT:
	case CURLOPT_CUSTOMREQUEST:
	case CURLOPT_COPYPOSTFIELDS: {
		if(!arg->is<VarStr>()) {
			vm.fail(loc, "expected a string as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		res = curl_easy_setopt(curl, (CURLoption)opt, as<VarStr>(arg)->getVal().c_str());
		break;
	}
	case CURLOPT_MIMEPOST: {
		if(!arg->is<VarMap>()) {
			vm.fail(loc,
				"expected a map of name-data pairs "
				"as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		curl_mime *mime = varCurl->createMime(vm, loc, as<VarMap>(arg));
		if(!mime) {
			vm.warn(loc,
				"failed to create mime from the given map (possibly empty map)");
			return nullptr;
		}
		res = curl_easy_setopt(curl, (CURLoption)opt, mime);
		break;
	}
	case CURLOPT_XFERINFOFUNCTION: {
		if(arg->is<VarNil>()) {
			varCurl->setProgressCB(vm.getMemoryManager(), nullptr, {});
			break;
		}
		if(!arg->is<VarFn>()) {
			vm.fail(loc, "expected a function as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		VarFn *f = as<VarFn>(arg);
		if(f->getParams().size() + f->getAssnParam().size() < 4) {
			vm.fail(loc, "expected function to have at least 4",
				" parameters for this option, found: ",
				f->getParams().size() + f->getAssnParam().size());
			return nullptr;
		}
		Span<Var *> cbArgs{args.begin() + 3, args.end()};
		varCurl->setProgressCB(vm.getMemoryManager(), f, cbArgs);
		break;
	}
	case CURLOPT_WRITEFUNCTION: {
		if(arg->is<VarNil>()) {
			varCurl->setWriteCB(vm.getMemoryManager(), nullptr, {});
			break;
		}
		if(!arg->is<VarFn>()) {
			vm.fail(loc, "expected a function as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		VarFn *f = as<VarFn>(arg);
		if(f->getParams().size() + f->getAssnParam().size() < 1) {
			vm.fail(loc, "expected function to have at least 1",
				" parameter for this option, found: ",
				f->getParams().size() + f->getAssnParam().size());
			return nullptr;
		}
		Span<Var *> cbArgs{args.begin() + 3, args.end()};
		varCurl->setWriteCB(vm.getMemoryManager(), f, cbArgs);
		break;
	}
	case CURLOPT_HTTPHEADER: {
		if(!arg->is<VarMap>()) {
			vm.fail(loc,
				"expected a map of name-data pairs "
				"as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		curl_slist *lst = varCurl->createSList(vm, loc, as<VarMap>(arg));
		if(!lst) {
			vm.warn(loc,
				"failed to create slist from the given map (possibly empty map)");
			return nullptr;
		}
		res = curl_easy_setopt(curl, (CURLoption)opt, lst);
		break;
	}
	default: {
		vm.fail(loc, "operation is not yet implemented");
		return nullptr;
	}
	}
	return vm.makeVar<VarInt>(loc, res);
}

INIT_MODULE(Curl)
{
	curl_global_init(CURL_GLOBAL_ALL);

	VarModule *mod = vm.getCurrModule();

	// Register the type names
	vm.registerType<VarCurl>(loc, "Curl");

	mod->addNativeFn(vm, "globalTrace", feralCurlGlobalTrace, 1);
	mod->addNativeFn(vm, "strerr", feralCurlEasyStrErrFromInt, 1);
	mod->addNativeFn(vm, "newEasy", feralCurlEasyInit);

	vm.addNativeTypeFn<VarCurl>(loc, "getInfoNative", feralCurlEasyGetInfoNative, 2);
	vm.addNativeTypeFn<VarCurl>(loc, "setOptNative", feralCurlEasySetOptNative, 2, true);
	vm.addNativeTypeFn<VarCurl>(loc, "perform", feralCurlEasyPerform, 0);
	vm.addNativeTypeFn<VarCurl>(loc, "setProgressCBTickNative", feralCurlSetProgressCBTick, 1);

	setEnumVars(vm, mod, loc);

	return true;
}

DEINIT_MODULE(Curl) { curl_global_cleanup(); }

void setEnumVars(VirtualMachine &vm, VarModule *mod, ModuleLoc loc)
{
	// All the enum values

	// CURLcode
	mod->addNativeVar("E_OK", vm.makeVar<VarInt>(loc, CURLE_OK));
	mod->addNativeVar("E_UNSUPPORTED_PROTOCOL",
			  vm.makeVar<VarInt>(loc, CURLE_UNSUPPORTED_PROTOCOL));
	mod->addNativeVar("E_FAILED_INIT", vm.makeVar<VarInt>(loc, CURLE_FAILED_INIT));
	mod->addNativeVar("E_URL_MALFORMAT", vm.makeVar<VarInt>(loc, CURLE_URL_MALFORMAT));
	mod->addNativeVar("E_NOT_BUILT_IN", vm.makeVar<VarInt>(loc, CURLE_NOT_BUILT_IN));
	mod->addNativeVar("E_COULDNT_RESOLVE_PROXY",
			  vm.makeVar<VarInt>(loc, CURLE_COULDNT_RESOLVE_PROXY));
	mod->addNativeVar("E_COULDNT_RESOLVE_HOST",
			  vm.makeVar<VarInt>(loc, CURLE_COULDNT_RESOLVE_HOST));
	mod->addNativeVar("E_COULDNT_CONNECT", vm.makeVar<VarInt>(loc, CURLE_COULDNT_CONNECT));
#if CURL_AT_LEAST_VERSION(7, 51, 0)
	mod->addNativeVar("E_WEIRD_SERVER_REPLY",
			  vm.makeVar<VarInt>(loc, CURLE_WEIRD_SERVER_REPLY));
#else
	mod->addNativeVar("E_FTP_WEIRD_SERVER_REPLY",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_SERVER_REPLY));
#endif
	mod->addNativeVar("E_REMOTE_ACCESS_DENIED",
			  vm.makeVar<VarInt>(loc, CURLE_REMOTE_ACCESS_DENIED));
	mod->addNativeVar("E_FTP_ACCEPT_FAILED", vm.makeVar<VarInt>(loc, CURLE_FTP_ACCEPT_FAILED));
	mod->addNativeVar("E_FTP_WEIRD_PASS_REPLY",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_PASS_REPLY));
	mod->addNativeVar("E_FTP_ACCEPT_TIMEOUT",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_ACCEPT_TIMEOUT));
	mod->addNativeVar("E_FTP_WEIRD_PASV_REPLY",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_PASV_REPLY));
	mod->addNativeVar("E_FTP_WEIRD_227_FORMAT",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_227_FORMAT));
	mod->addNativeVar("E_FTP_CANT_GET_HOST", vm.makeVar<VarInt>(loc, CURLE_FTP_CANT_GET_HOST));
	mod->addNativeVar("E_HTTP2", vm.makeVar<VarInt>(loc, CURLE_HTTP2));
	mod->addNativeVar("E_FTP_COULDNT_SET_TYPE",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_COULDNT_SET_TYPE));
	mod->addNativeVar("E_PARTIAL_FILE", vm.makeVar<VarInt>(loc, CURLE_PARTIAL_FILE));
	mod->addNativeVar("E_FTP_COULDNT_RETR_FILE",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_COULDNT_RETR_FILE));
	mod->addNativeVar("E_OBSOLETE20", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE20));
	mod->addNativeVar("E_QUOTE_ERROR", vm.makeVar<VarInt>(loc, CURLE_QUOTE_ERROR));
	mod->addNativeVar("E_HTTP_RETURNED_ERROR",
			  vm.makeVar<VarInt>(loc, CURLE_HTTP_RETURNED_ERROR));
	mod->addNativeVar("E_WRITE_ERROR", vm.makeVar<VarInt>(loc, CURLE_WRITE_ERROR));
	mod->addNativeVar("E_OBSOLETE24", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE24));
	mod->addNativeVar("E_UPLOAD_FAILED", vm.makeVar<VarInt>(loc, CURLE_UPLOAD_FAILED));
	mod->addNativeVar("E_READ_ERROR", vm.makeVar<VarInt>(loc, CURLE_READ_ERROR));
	mod->addNativeVar("E_OUT_OF_MEMORY", vm.makeVar<VarInt>(loc, CURLE_OUT_OF_MEMORY));
	mod->addNativeVar("E_OPERATION_TIMEDOUT",
			  vm.makeVar<VarInt>(loc, CURLE_OPERATION_TIMEDOUT));
	mod->addNativeVar("E_OBSOLETE29", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE29));
	mod->addNativeVar("E_FTP_PORT_FAILED", vm.makeVar<VarInt>(loc, CURLE_FTP_PORT_FAILED));
	mod->addNativeVar("E_FTP_COULDNT_USE_REST",
			  vm.makeVar<VarInt>(loc, CURLE_FTP_COULDNT_USE_REST));
	mod->addNativeVar("E_OBSOLETE32", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE32));
	mod->addNativeVar("E_RANGE_ERROR", vm.makeVar<VarInt>(loc, CURLE_RANGE_ERROR));
	mod->addNativeVar("E_HTTP_POST_ERROR", vm.makeVar<VarInt>(loc, CURLE_HTTP_POST_ERROR));
	mod->addNativeVar("E_SSL_CONNECT_ERROR", vm.makeVar<VarInt>(loc, CURLE_SSL_CONNECT_ERROR));
	mod->addNativeVar("E_BAD_DOWNLOAD_RESUME",
			  vm.makeVar<VarInt>(loc, CURLE_BAD_DOWNLOAD_RESUME));
	mod->addNativeVar("E_FILE_COULDNT_READ_FILE",
			  vm.makeVar<VarInt>(loc, CURLE_FILE_COULDNT_READ_FILE));
	mod->addNativeVar("E_LDAP_CANNOT_BIND", vm.makeVar<VarInt>(loc, CURLE_LDAP_CANNOT_BIND));
	mod->addNativeVar("E_LDAP_SEARCH_FAILED",
			  vm.makeVar<VarInt>(loc, CURLE_LDAP_SEARCH_FAILED));
	mod->addNativeVar("E_OBSOLETE40", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE40));
	mod->addNativeVar("E_FUNCTION_NOT_FOUND",
			  vm.makeVar<VarInt>(loc, CURLE_FUNCTION_NOT_FOUND));
	mod->addNativeVar("E_ABORTED_BY_CALLBACK",
			  vm.makeVar<VarInt>(loc, CURLE_ABORTED_BY_CALLBACK));
	mod->addNativeVar("E_BAD_FUNCTION_ARGUMENT",
			  vm.makeVar<VarInt>(loc, CURLE_BAD_FUNCTION_ARGUMENT));
	mod->addNativeVar("E_OBSOLETE44", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE44));
	mod->addNativeVar("E_INTERFACE_FAILED", vm.makeVar<VarInt>(loc, CURLE_INTERFACE_FAILED));
	mod->addNativeVar("E_OBSOLETE46", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE46));
	mod->addNativeVar("E_TOO_MANY_REDIRECTS",
			  vm.makeVar<VarInt>(loc, CURLE_TOO_MANY_REDIRECTS));
	mod->addNativeVar("E_UNKNOWN_OPTION", vm.makeVar<VarInt>(loc, CURLE_UNKNOWN_OPTION));
	mod->addNativeVar("E_TELNET_OPTION_SYNTAX",
			  vm.makeVar<VarInt>(loc, CURLE_TELNET_OPTION_SYNTAX));
	mod->addNativeVar("E_OBSOLETE50", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE50));
#if CURL_AT_LEAST_VERSION(7, 62, 0)
	mod->addNativeVar("E_OBSOLETE51", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE51));
#endif
	mod->addNativeVar("E_GOT_NOTHING", vm.makeVar<VarInt>(loc, CURLE_GOT_NOTHING));
	mod->addNativeVar("E_SSL_ENGINE_NOTFOUND",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_ENGINE_NOTFOUND));
	mod->addNativeVar("E_SSL_ENGINE_SETFAILED",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_ENGINE_SETFAILED));
	mod->addNativeVar("E_SEND_ERROR", vm.makeVar<VarInt>(loc, CURLE_SEND_ERROR));
	mod->addNativeVar("E_RECV_ERROR", vm.makeVar<VarInt>(loc, CURLE_RECV_ERROR));
	mod->addNativeVar("E_OBSOLETE57", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE57));
	mod->addNativeVar("E_SSL_CERTPROBLEM", vm.makeVar<VarInt>(loc, CURLE_SSL_CERTPROBLEM));
	mod->addNativeVar("E_SSL_CIPHER", vm.makeVar<VarInt>(loc, CURLE_SSL_CIPHER));
	mod->addNativeVar("E_PEER_FAILED_VERIFICATION",
			  vm.makeVar<VarInt>(loc, CURLE_PEER_FAILED_VERIFICATION));
	mod->addNativeVar("E_BAD_CONTENT_ENCODING",
			  vm.makeVar<VarInt>(loc, CURLE_BAD_CONTENT_ENCODING));
	mod->addNativeVar("E_LDAP_INVALID_URL", vm.makeVar<VarInt>(loc, CURLE_LDAP_INVALID_URL));
	mod->addNativeVar("E_FILESIZE_EXCEEDED", vm.makeVar<VarInt>(loc, CURLE_FILESIZE_EXCEEDED));
	mod->addNativeVar("E_USE_SSL_FAILED", vm.makeVar<VarInt>(loc, CURLE_USE_SSL_FAILED));
	mod->addNativeVar("E_SEND_FAIL_REWIND", vm.makeVar<VarInt>(loc, CURLE_SEND_FAIL_REWIND));
	mod->addNativeVar("E_SSL_ENGINE_INITFAILED",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_ENGINE_INITFAILED));
	mod->addNativeVar("E_LOGIN_DENIED", vm.makeVar<VarInt>(loc, CURLE_LOGIN_DENIED));
	mod->addNativeVar("E_TFTP_NOTFOUND", vm.makeVar<VarInt>(loc, CURLE_TFTP_NOTFOUND));
	mod->addNativeVar("E_TFTP_PERM", vm.makeVar<VarInt>(loc, CURLE_TFTP_PERM));
	mod->addNativeVar("E_REMOTE_DISK_FULL", vm.makeVar<VarInt>(loc, CURLE_REMOTE_DISK_FULL));
	mod->addNativeVar("E_TFTP_ILLEGAL", vm.makeVar<VarInt>(loc, CURLE_TFTP_ILLEGAL));
	mod->addNativeVar("E_TFTP_UNKNOWNID", vm.makeVar<VarInt>(loc, CURLE_TFTP_UNKNOWNID));
	mod->addNativeVar("E_REMOTE_FILE_EXISTS",
			  vm.makeVar<VarInt>(loc, CURLE_REMOTE_FILE_EXISTS));
	mod->addNativeVar("E_TFTP_NOSUCHUSER", vm.makeVar<VarInt>(loc, CURLE_TFTP_NOSUCHUSER));
	mod->addNativeVar("E_CONV_FAILED", vm.makeVar<VarInt>(loc, CURLE_CONV_FAILED));
	mod->addNativeVar("E_CONV_REQD", vm.makeVar<VarInt>(loc, CURLE_CONV_REQD));
	mod->addNativeVar("E_SSL_CACERT_BADFILE",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_CACERT_BADFILE));
	mod->addNativeVar("E_REMOTE_FILE_NOT_FOUND",
			  vm.makeVar<VarInt>(loc, CURLE_REMOTE_FILE_NOT_FOUND));
	mod->addNativeVar("E_SSH", vm.makeVar<VarInt>(loc, CURLE_SSH));
	mod->addNativeVar("E_SSL_SHUTDOWN_FAILED",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_SHUTDOWN_FAILED));
	mod->addNativeVar("E_AGAIN", vm.makeVar<VarInt>(loc, CURLE_AGAIN));
	mod->addNativeVar("E_SSL_CRL_BADFILE", vm.makeVar<VarInt>(loc, CURLE_SSL_CRL_BADFILE));
	mod->addNativeVar("E_SSL_ISSUER_ERROR", vm.makeVar<VarInt>(loc, CURLE_SSL_ISSUER_ERROR));
	mod->addNativeVar("E_FTP_PRET_FAILED", vm.makeVar<VarInt>(loc, CURLE_FTP_PRET_FAILED));
	mod->addNativeVar("E_RTSP_CSEQ_ERROR", vm.makeVar<VarInt>(loc, CURLE_RTSP_CSEQ_ERROR));
	mod->addNativeVar("E_RTSP_SESSION_ERROR",
			  vm.makeVar<VarInt>(loc, CURLE_RTSP_SESSION_ERROR));
	mod->addNativeVar("E_FTP_BAD_FILE_LIST", vm.makeVar<VarInt>(loc, CURLE_FTP_BAD_FILE_LIST));
	mod->addNativeVar("E_CHUNK_FAILED", vm.makeVar<VarInt>(loc, CURLE_CHUNK_FAILED));
	mod->addNativeVar("E_NO_CONNECTION_AVAILABLE",
			  vm.makeVar<VarInt>(loc, CURLE_NO_CONNECTION_AVAILABLE));
	mod->addNativeVar("E_SSL_PINNEDPUBKEYNOTMATCH",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_PINNEDPUBKEYNOTMATCH));
	mod->addNativeVar("E_SSL_INVALIDCERTSTATUS",
			  vm.makeVar<VarInt>(loc, CURLE_SSL_INVALIDCERTSTATUS));
#if CURL_AT_LEAST_VERSION(7, 50, 2)
	mod->addNativeVar("E_HTTP2_STREAM", vm.makeVar<VarInt>(loc, CURLE_HTTP2_STREAM));
#endif
#if CURL_AT_LEAST_VERSION(7, 59, 0)
	mod->addNativeVar("E_RECURSIVE_API_CALL",
			  vm.makeVar<VarInt>(loc, CURLE_RECURSIVE_API_CALL));
#endif
#if CURL_AT_LEAST_VERSION(7, 66, 0)
	mod->addNativeVar("E_AUTH_ERROR", vm.makeVar<VarInt>(loc, CURLE_AUTH_ERROR));
#endif
#if CURL_AT_LEAST_VERSION(7, 68, 0)
	mod->addNativeVar("E_HTTP3", vm.makeVar<VarInt>(loc, CURLE_HTTP3));
#endif
#if CURL_AT_LEAST_VERSION(7, 69, 0)
	mod->addNativeVar("E_QUIC_CONNECT_ERROR",
			  vm.makeVar<VarInt>(loc, CURLE_QUIC_CONNECT_ERROR));
#endif

	// CURLMcode
	mod->addNativeVar("M_CALL_MULTI_PERFORM",
			  vm.makeVar<VarInt>(loc, CURLM_CALL_MULTI_PERFORM));
	mod->addNativeVar("M_OK", vm.makeVar<VarInt>(loc, CURLM_OK));
	mod->addNativeVar("M_BAD_HANDLE", vm.makeVar<VarInt>(loc, CURLM_BAD_HANDLE));
	mod->addNativeVar("M_BAD_EASY_HANDLE", vm.makeVar<VarInt>(loc, CURLM_BAD_EASY_HANDLE));
	mod->addNativeVar("M_OUT_OF_MEMORY", vm.makeVar<VarInt>(loc, CURLM_OUT_OF_MEMORY));
	mod->addNativeVar("M_INTERNAL_ERROR", vm.makeVar<VarInt>(loc, CURLM_INTERNAL_ERROR));
	mod->addNativeVar("M_BAD_SOCKET", vm.makeVar<VarInt>(loc, CURLM_BAD_SOCKET));
	mod->addNativeVar("M_UNKNOWN_OPTION", vm.makeVar<VarInt>(loc, CURLM_UNKNOWN_OPTION));
	mod->addNativeVar("M_ADDED_ALREADY", vm.makeVar<VarInt>(loc, CURLM_ADDED_ALREADY));
#if CURL_AT_LEAST_VERSION(7, 59, 0)
	mod->addNativeVar("M_RECURSIVE_API_CALL",
			  vm.makeVar<VarInt>(loc, CURLM_RECURSIVE_API_CALL));
#endif
#if CURL_AT_LEAST_VERSION(7, 68, 0)
	mod->addNativeVar("WAKEUP_FAILURE", vm.makeVar<VarInt>(loc, CURLM_WAKEUP_FAILURE));
#endif
#if CURL_AT_LEAST_VERSION(7, 69, 0)
	mod->addNativeVar("BAD_FUNCTION_ARGUMENT",
			  vm.makeVar<VarInt>(loc, CURLM_BAD_FUNCTION_ARGUMENT));
#endif

	// CURLSHcode
	mod->addNativeVar("SHE_OK", vm.makeVar<VarInt>(loc, CURLSHE_OK));
	mod->addNativeVar("SHE_BAD_OPTION", vm.makeVar<VarInt>(loc, CURLSHE_BAD_OPTION));
	mod->addNativeVar("SHE_IN_USE", vm.makeVar<VarInt>(loc, CURLSHE_IN_USE));
	mod->addNativeVar("SHE_INVALID", vm.makeVar<VarInt>(loc, CURLSHE_INVALID));
	mod->addNativeVar("SHE_NOMEM", vm.makeVar<VarInt>(loc, CURLSHE_NOMEM));
	mod->addNativeVar("SHE_NOT_BUILT_IN", vm.makeVar<VarInt>(loc, CURLSHE_NOT_BUILT_IN));

#if CURL_AT_LEAST_VERSION(7, 62, 0)
	// CURLUcode
	mod->addNativeVar("UE_OK", vm.makeVar<VarInt>(loc, CURLUE_OK));
	mod->addNativeVar("UE_BAD_HANDLE", vm.makeVar<VarInt>(loc, CURLUE_BAD_HANDLE));
	mod->addNativeVar("UE_BAD_PARTPOINTER", vm.makeVar<VarInt>(loc, CURLUE_BAD_PARTPOINTER));
	mod->addNativeVar("UE_MALFORMED_INPUT", vm.makeVar<VarInt>(loc, CURLUE_MALFORMED_INPUT));
	mod->addNativeVar("UE_BAD_PORT_NUMBER", vm.makeVar<VarInt>(loc, CURLUE_BAD_PORT_NUMBER));
	mod->addNativeVar("UE_UNSUPPORTED_SCHEME",
			  vm.makeVar<VarInt>(loc, CURLUE_UNSUPPORTED_SCHEME));
	mod->addNativeVar("UE_URLDECODE", vm.makeVar<VarInt>(loc, CURLUE_URLDECODE));
	mod->addNativeVar("UE_OUT_OF_MEMORY", vm.makeVar<VarInt>(loc, CURLUE_OUT_OF_MEMORY));
	mod->addNativeVar("UE_USER_NOT_ALLOWED", vm.makeVar<VarInt>(loc, CURLUE_USER_NOT_ALLOWED));
	mod->addNativeVar("UE_UNKNOWN_PART", vm.makeVar<VarInt>(loc, CURLUE_UNKNOWN_PART));
	mod->addNativeVar("UE_NO_SCHEME", vm.makeVar<VarInt>(loc, CURLUE_NO_SCHEME));
	mod->addNativeVar("UE_NO_USER", vm.makeVar<VarInt>(loc, CURLUE_NO_USER));
	mod->addNativeVar("UE_NO_PASSWORD", vm.makeVar<VarInt>(loc, CURLUE_NO_PASSWORD));
	mod->addNativeVar("UE_NO_OPTIONS", vm.makeVar<VarInt>(loc, CURLUE_NO_OPTIONS));
	mod->addNativeVar("UE_NO_HOST", vm.makeVar<VarInt>(loc, CURLUE_NO_HOST));
	mod->addNativeVar("UE_NO_PORT", vm.makeVar<VarInt>(loc, CURLUE_NO_PORT));
	mod->addNativeVar("UE_NO_QUERY", vm.makeVar<VarInt>(loc, CURLUE_NO_QUERY));
	mod->addNativeVar("UE_NO_FRAGMENT", vm.makeVar<VarInt>(loc, CURLUE_NO_FRAGMENT));
#endif

	// EASY_OPTS

	// BEHAVIOR OPTIONS
	mod->addNativeVar("OPT_VERBOSE", vm.makeVar<VarInt>(loc, CURLOPT_VERBOSE));
	mod->addNativeVar("OPT_HEADER", vm.makeVar<VarInt>(loc, CURLOPT_HEADER));
	mod->addNativeVar("OPT_NOPROGRESS", vm.makeVar<VarInt>(loc, CURLOPT_NOPROGRESS));
	mod->addNativeVar("OPT_NOSIGNAL", vm.makeVar<VarInt>(loc, CURLOPT_NOSIGNAL));
	mod->addNativeVar("OPT_WILDCARDMATCH", vm.makeVar<VarInt>(loc, CURLOPT_WILDCARDMATCH));

	// CALLBACK OPTIONS
	mod->addNativeVar("OPT_WRITEFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_WRITEFUNCTION));
	mod->addNativeVar("OPT_WRITEDATA", vm.makeVar<VarInt>(loc, CURLOPT_WRITEDATA));
	mod->addNativeVar("OPT_READFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_READFUNCTION));
	mod->addNativeVar("OPT_READDATA", vm.makeVar<VarInt>(loc, CURLOPT_READDATA));
	mod->addNativeVar("OPT_SEEKFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_SEEKFUNCTION));
	mod->addNativeVar("OPT_SEEKDATA", vm.makeVar<VarInt>(loc, CURLOPT_SEEKDATA));
	mod->addNativeVar("OPT_SOCKOPTFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_SOCKOPTFUNCTION));
	mod->addNativeVar("OPT_SOCKOPTDATA", vm.makeVar<VarInt>(loc, CURLOPT_SOCKOPTDATA));
	mod->addNativeVar("OPT_OPENSOCKETFUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_OPENSOCKETFUNCTION));
	mod->addNativeVar("OPT_OPENSOCKETDATA", vm.makeVar<VarInt>(loc, CURLOPT_OPENSOCKETDATA));
	mod->addNativeVar("OPT_CLOSESOCKETFUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_CLOSESOCKETFUNCTION));
	mod->addNativeVar("OPT_CLOSESOCKETDATA", vm.makeVar<VarInt>(loc, CURLOPT_CLOSESOCKETDATA));
	mod->addNativeVar("OPT_PROGRESSDATA", vm.makeVar<VarInt>(loc, CURLOPT_PROGRESSDATA));
	mod->addNativeVar("OPT_XFERINFOFUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_XFERINFOFUNCTION));
	mod->addNativeVar("OPT_XFERINFODATA", vm.makeVar<VarInt>(loc, CURLOPT_XFERINFODATA));
	mod->addNativeVar("OPT_HEADERFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_HEADERFUNCTION));
	mod->addNativeVar("OPT_HEADERDATA", vm.makeVar<VarInt>(loc, CURLOPT_HEADERDATA));
	mod->addNativeVar("OPT_DEBUGFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_DEBUGFUNCTION));
	mod->addNativeVar("OPT_DEBUGDATA", vm.makeVar<VarInt>(loc, CURLOPT_DEBUGDATA));
	mod->addNativeVar("OPT_SSL_CTX_FUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSL_CTX_FUNCTION));
	mod->addNativeVar("OPT_SSL_CTX_DATA", vm.makeVar<VarInt>(loc, CURLOPT_SSL_CTX_DATA));
	mod->addNativeVar("OPT_INTERLEAVEFUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_INTERLEAVEFUNCTION));
	mod->addNativeVar("OPT_INTERLEAVEDATA", vm.makeVar<VarInt>(loc, CURLOPT_INTERLEAVEDATA));
	mod->addNativeVar("OPT_CHUNK_BGN_FUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_CHUNK_BGN_FUNCTION));
	mod->addNativeVar("OPT_CHUNK_END_FUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_CHUNK_END_FUNCTION));
	mod->addNativeVar("OPT_CHUNK_DATA", vm.makeVar<VarInt>(loc, CURLOPT_CHUNK_DATA));
	mod->addNativeVar("OPT_FNMATCH_FUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_FNMATCH_FUNCTION));
	mod->addNativeVar("OPT_FNMATCH_DATA", vm.makeVar<VarInt>(loc, CURLOPT_FNMATCH_DATA));
#if CURL_AT_LEAST_VERSION(7, 54, 0)
	mod->addNativeVar("OPT_SUPPRESS_CONNECT_HEADERS",
			  vm.makeVar<VarInt>(loc, CURLOPT_SUPPRESS_CONNECT_HEADERS));
#endif
#if CURL_AT_LEAST_VERSION(7, 59, 0)
	mod->addNativeVar("OPT_RESOLVER_START_FUNCTION",
			  vm.makeVar<VarInt>(loc, CURLOPT_RESOLVER_START_FUNCTION));
	mod->addNativeVar("OPT_RESOLVER_START_DATA",
			  vm.makeVar<VarInt>(loc, CURLOPT_RESOLVER_START_DATA));
#endif

	// ERROR OPTIONS
	mod->addNativeVar("OPT_ERRORBUFFER", vm.makeVar<VarInt>(loc, CURLOPT_ERRORBUFFER));
	mod->addNativeVar("OPT_STDERR", vm.makeVar<VarInt>(loc, CURLOPT_STDERR));
	mod->addNativeVar("OPT_FAILONERROR", vm.makeVar<VarInt>(loc, CURLOPT_FAILONERROR));
#if CURL_AT_LEAST_VERSION(7, 51, 0)
	mod->addNativeVar("OPT_KEEP_SENDING_ON_ERROR",
			  vm.makeVar<VarInt>(loc, CURLOPT_KEEP_SENDING_ON_ERROR));
#endif

	// NETWORK OPTIONS
	mod->addNativeVar("OPT_URL", vm.makeVar<VarInt>(loc, CURLOPT_URL));
	mod->addNativeVar("OPT_PATH_AS_IS", vm.makeVar<VarInt>(loc, CURLOPT_PATH_AS_IS));
	mod->addNativeVar("OPT_PROTOCOLS_STR", vm.makeVar<VarInt>(loc, CURLOPT_PROTOCOLS_STR));
	mod->addNativeVar("OPT_REDIR_PROTOCOLS_STR",
			  vm.makeVar<VarInt>(loc, CURLOPT_REDIR_PROTOCOLS_STR));
	mod->addNativeVar("OPT_DEFAULT_PROTOCOL",
			  vm.makeVar<VarInt>(loc, CURLOPT_DEFAULT_PROTOCOL));
	mod->addNativeVar("OPT_PROXY", vm.makeVar<VarInt>(loc, CURLOPT_PROXY));
#if CURL_AT_LEAST_VERSION(7, 52, 0)
	mod->addNativeVar("OPT_PRE_PROXY", vm.makeVar<VarInt>(loc, CURLOPT_PRE_PROXY));
#endif
	mod->addNativeVar("OPT_PROXYPORT", vm.makeVar<VarInt>(loc, CURLOPT_PROXYPORT));
	mod->addNativeVar("OPT_PROXYTYPE", vm.makeVar<VarInt>(loc, CURLOPT_PROXYTYPE));
	mod->addNativeVar("OPT_NOPROXY", vm.makeVar<VarInt>(loc, CURLOPT_NOPROXY));
	mod->addNativeVar("OPT_HTTPPROXYTUNNEL", vm.makeVar<VarInt>(loc, CURLOPT_HTTPPROXYTUNNEL));
#if CURL_AT_LEAST_VERSION(7, 49, 0)
	mod->addNativeVar("OPT_CONNECT_TO", vm.makeVar<VarInt>(loc, CURLOPT_CONNECT_TO));
#endif
#if CURL_AT_LEAST_VERSION(7, 55, 0)
	mod->addNativeVar("OPT_SOCKS5_AUTH", vm.makeVar<VarInt>(loc, CURLOPT_SOCKS5_AUTH));
#endif
	mod->addNativeVar("OPT_SOCKS5_GSSAPI_NEC",
			  vm.makeVar<VarInt>(loc, CURLOPT_SOCKS5_GSSAPI_NEC));
	mod->addNativeVar("OPT_PROXY_SERVICE_NAME",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SERVICE_NAME));
#if CURL_AT_LEAST_VERSION(7, 60, 0)
	mod->addNativeVar("OPT_HAPROXYPROTOCOL", vm.makeVar<VarInt>(loc, CURLOPT_HAPROXYPROTOCOL));
#endif
	mod->addNativeVar("OPT_SERVICE_NAME", vm.makeVar<VarInt>(loc, CURLOPT_SERVICE_NAME));
	mod->addNativeVar("OPT_INTERFACE", vm.makeVar<VarInt>(loc, CURLOPT_INTERFACE));
	mod->addNativeVar("OPT_LOCALPORT", vm.makeVar<VarInt>(loc, CURLOPT_LOCALPORT));
	mod->addNativeVar("OPT_LOCALPORTRANGE", vm.makeVar<VarInt>(loc, CURLOPT_LOCALPORTRANGE));
	mod->addNativeVar("OPT_DNS_CACHE_TIMEOUT",
			  vm.makeVar<VarInt>(loc, CURLOPT_DNS_CACHE_TIMEOUT));
#if CURL_AT_LEAST_VERSION(7, 62, 0)
	mod->addNativeVar("OPT_DOH_URL", vm.makeVar<VarInt>(loc, CURLOPT_DOH_URL));
#endif
	mod->addNativeVar("OPT_BUFFERSIZE", vm.makeVar<VarInt>(loc, CURLOPT_BUFFERSIZE));
	mod->addNativeVar("OPT_PORT", vm.makeVar<VarInt>(loc, CURLOPT_PORT));
#if CURL_AT_LEAST_VERSION(7, 49, 0)
	mod->addNativeVar("OPT_TCP_FASTOPEN", vm.makeVar<VarInt>(loc, CURLOPT_TCP_FASTOPEN));
#endif
	mod->addNativeVar("OPT_TCP_NODELAY", vm.makeVar<VarInt>(loc, CURLOPT_TCP_NODELAY));
	mod->addNativeVar("OPT_ADDRESS_SCOPE", vm.makeVar<VarInt>(loc, CURLOPT_ADDRESS_SCOPE));
	mod->addNativeVar("OPT_TCP_KEEPALIVE", vm.makeVar<VarInt>(loc, CURLOPT_TCP_KEEPALIVE));
	mod->addNativeVar("OPT_TCP_KEEPIDLE", vm.makeVar<VarInt>(loc, CURLOPT_TCP_KEEPIDLE));
	mod->addNativeVar("OPT_TCP_KEEPINTVL", vm.makeVar<VarInt>(loc, CURLOPT_TCP_KEEPINTVL));
	mod->addNativeVar("OPT_UNIX_SOCKET_PATH",
			  vm.makeVar<VarInt>(loc, CURLOPT_UNIX_SOCKET_PATH));
#if CURL_AT_LEAST_VERSION(7, 53, 0)
	mod->addNativeVar("OPT_ABSTRACT_UNIX_SOCKET",
			  vm.makeVar<VarInt>(loc, CURLOPT_ABSTRACT_UNIX_SOCKET));
#endif

	// NAMES and PASSWORDS OPTIONS (Authentication)
	mod->addNativeVar("OPT_NETRC", vm.makeVar<VarInt>(loc, CURLOPT_NETRC));
	mod->addNativeVar("OPT_NETRC_FILE", vm.makeVar<VarInt>(loc, CURLOPT_NETRC_FILE));
	mod->addNativeVar("OPT_USERPWD", vm.makeVar<VarInt>(loc, CURLOPT_USERPWD));
	mod->addNativeVar("OPT_PROXYUSERPWD", vm.makeVar<VarInt>(loc, CURLOPT_PROXYUSERPWD));
	mod->addNativeVar("OPT_USERNAME", vm.makeVar<VarInt>(loc, CURLOPT_USERNAME));
	mod->addNativeVar("OPT_PASSWORD", vm.makeVar<VarInt>(loc, CURLOPT_PASSWORD));
	mod->addNativeVar("OPT_LOGIN_OPTIONS", vm.makeVar<VarInt>(loc, CURLOPT_LOGIN_OPTIONS));
	mod->addNativeVar("OPT_PROXYUSERNAME", vm.makeVar<VarInt>(loc, CURLOPT_PROXYUSERNAME));
	mod->addNativeVar("OPT_PROXYPASSWORD", vm.makeVar<VarInt>(loc, CURLOPT_PROXYPASSWORD));
	mod->addNativeVar("OPT_HTTPAUTH", vm.makeVar<VarInt>(loc, CURLOPT_HTTPAUTH));
	mod->addNativeVar("OPT_TLSAUTH_USERNAME",
			  vm.makeVar<VarInt>(loc, CURLOPT_TLSAUTH_USERNAME));
	mod->addNativeVar("OPT_TLSAUTH_PASSWORD",
			  vm.makeVar<VarInt>(loc, CURLOPT_TLSAUTH_PASSWORD));
	mod->addNativeVar("OPT_TLSAUTH_TYPE", vm.makeVar<VarInt>(loc, CURLOPT_TLSAUTH_TYPE));
#if CURL_AT_LEAST_VERSION(7, 52, 0)
	mod->addNativeVar("OPT_PROXY_TLSAUTH_USERNAME",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLSAUTH_USERNAME));
	mod->addNativeVar("OPT_PROXY_TLSAUTH_PASSWORD",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLSAUTH_PASSWORD));
	mod->addNativeVar("OPT_PROXY_TLSAUTH_TYPE",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLSAUTH_TYPE));
#endif
	mod->addNativeVar("OPT_PROXYAUTH", vm.makeVar<VarInt>(loc, CURLOPT_PROXYAUTH));
#if CURL_AT_LEAST_VERSION(7, 66, 0)
	mod->addNativeVar("OPT_SASL_AUTHZID", vm.makeVar<VarInt>(loc, CURLOPT_SASL_AUTHZID));
#endif
#if CURL_AT_LEAST_VERSION(7, 61, 0)
	mod->addNativeVar("OPT_SASL_IR", vm.makeVar<VarInt>(loc, CURLOPT_SASL_IR));
	mod->addNativeVar("OPT_DISALLOW_USERNAME_IN_URL",
			  vm.makeVar<VarInt>(loc, CURLOPT_DISALLOW_USERNAME_IN_URL));
#endif
	mod->addNativeVar("OPT_XOAUTH2_BEARER", vm.makeVar<VarInt>(loc, CURLOPT_XOAUTH2_BEARER));

	// HTTP OPTIONS
	mod->addNativeVar("OPT_AUTOREFERER", vm.makeVar<VarInt>(loc, CURLOPT_AUTOREFERER));
	mod->addNativeVar("OPT_ACCEPT_ENCODING", vm.makeVar<VarInt>(loc, CURLOPT_ACCEPT_ENCODING));
	mod->addNativeVar("OPT_TRANSFER_ENCODING",
			  vm.makeVar<VarInt>(loc, CURLOPT_TRANSFER_ENCODING));
	mod->addNativeVar("OPT_FOLLOWLOCATION", vm.makeVar<VarInt>(loc, CURLOPT_FOLLOWLOCATION));
	mod->addNativeVar("OPT_UNRESTRICTED_AUTH",
			  vm.makeVar<VarInt>(loc, CURLOPT_UNRESTRICTED_AUTH));
	mod->addNativeVar("OPT_MAXREDIRS", vm.makeVar<VarInt>(loc, CURLOPT_MAXREDIRS));
	mod->addNativeVar("OPT_POSTREDIR", vm.makeVar<VarInt>(loc, CURLOPT_POSTREDIR));
	mod->addNativeVar("OPT_POST", vm.makeVar<VarInt>(loc, CURLOPT_POST));
	mod->addNativeVar("OPT_POSTFIELDS", vm.makeVar<VarInt>(loc, CURLOPT_POSTFIELDS));
	mod->addNativeVar("OPT_POSTFIELDSIZE", vm.makeVar<VarInt>(loc, CURLOPT_POSTFIELDSIZE));
	mod->addNativeVar("OPT_POSTFIELDSIZE_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_POSTFIELDSIZE_LARGE));
	mod->addNativeVar("OPT_COPYPOSTFIELDS", vm.makeVar<VarInt>(loc, CURLOPT_COPYPOSTFIELDS));
	mod->addNativeVar("OPT_REFERER", vm.makeVar<VarInt>(loc, CURLOPT_REFERER));
	mod->addNativeVar("OPT_USERAGENT", vm.makeVar<VarInt>(loc, CURLOPT_USERAGENT));
	mod->addNativeVar("OPT_HTTPHEADER", vm.makeVar<VarInt>(loc, CURLOPT_HTTPHEADER));
	mod->addNativeVar("OPT_HEADEROPT", vm.makeVar<VarInt>(loc, CURLOPT_HEADEROPT));
	mod->addNativeVar("OPT_PROXYHEADER", vm.makeVar<VarInt>(loc, CURLOPT_PROXYHEADER));
	mod->addNativeVar("OPT_HTTP200ALIASES", vm.makeVar<VarInt>(loc, CURLOPT_HTTP200ALIASES));
	mod->addNativeVar("OPT_COOKIE", vm.makeVar<VarInt>(loc, CURLOPT_COOKIE));
	mod->addNativeVar("OPT_COOKIEFILE", vm.makeVar<VarInt>(loc, CURLOPT_COOKIEFILE));
	mod->addNativeVar("OPT_COOKIEJAR", vm.makeVar<VarInt>(loc, CURLOPT_COOKIEJAR));
	mod->addNativeVar("OPT_COOKIESESSION", vm.makeVar<VarInt>(loc, CURLOPT_COOKIESESSION));
	mod->addNativeVar("OPT_COOKIELIST", vm.makeVar<VarInt>(loc, CURLOPT_COOKIELIST));
#if CURL_AT_LEAST_VERSION(7, 64, 1)
	mod->addNativeVar("OPT_ALTSVC", vm.makeVar<VarInt>(loc, CURLOPT_ALTSVC));
	mod->addNativeVar("OPT_ALTSVC_CTRL", vm.makeVar<VarInt>(loc, CURLOPT_ALTSVC_CTRL));
#endif
	mod->addNativeVar("OPT_HTTPGET", vm.makeVar<VarInt>(loc, CURLOPT_HTTPGET));
#if CURL_AT_LEAST_VERSION(7, 55, 0)
	mod->addNativeVar("OPT_REQUEST_TARGET", vm.makeVar<VarInt>(loc, CURLOPT_REQUEST_TARGET));
#endif
	mod->addNativeVar("OPT_HTTP_VERSION", vm.makeVar<VarInt>(loc, CURLOPT_HTTP_VERSION));
#if CURL_AT_LEAST_VERSION(7, 64, 0)
	mod->addNativeVar("OPT_HTTP09_ALLOWED", vm.makeVar<VarInt>(loc, CURLOPT_HTTP09_ALLOWED));
	mod->addNativeVar("OPT_TRAILERFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_TRAILERFUNCTION));
	mod->addNativeVar("OPT_TRAILERDATA", vm.makeVar<VarInt>(loc, CURLOPT_TRAILERDATA));
#endif
	mod->addNativeVar("OPT_IGNORE_CONTENT_LENGTH",
			  vm.makeVar<VarInt>(loc, CURLOPT_IGNORE_CONTENT_LENGTH));
	mod->addNativeVar("OPT_HTTP_CONTENT_DECODING",
			  vm.makeVar<VarInt>(loc, CURLOPT_HTTP_CONTENT_DECODING));
	mod->addNativeVar("OPT_HTTP_TRANSFER_DECODING",
			  vm.makeVar<VarInt>(loc, CURLOPT_HTTP_TRANSFER_DECODING));
	mod->addNativeVar("OPT_EXPECT_100_TIMEOUT_MS",
			  vm.makeVar<VarInt>(loc, CURLOPT_EXPECT_100_TIMEOUT_MS));
	mod->addNativeVar("OPT_PIPEWAIT", vm.makeVar<VarInt>(loc, CURLOPT_PIPEWAIT));
	mod->addNativeVar("OPT_STREAM_DEPENDS", vm.makeVar<VarInt>(loc, CURLOPT_STREAM_DEPENDS));
	mod->addNativeVar("OPT_STREAM_DEPENDS_E",
			  vm.makeVar<VarInt>(loc, CURLOPT_STREAM_DEPENDS_E));
	mod->addNativeVar("OPT_STREAM_WEIGHT", vm.makeVar<VarInt>(loc, CURLOPT_STREAM_WEIGHT));

	// SMTP OPTIONS
	mod->addNativeVar("OPT_MAIL_FROM", vm.makeVar<VarInt>(loc, CURLOPT_MAIL_FROM));
	mod->addNativeVar("OPT_MAIL_RCPT", vm.makeVar<VarInt>(loc, CURLOPT_MAIL_RCPT));
	mod->addNativeVar("OPT_MAIL_AUTH", vm.makeVar<VarInt>(loc, CURLOPT_MAIL_AUTH));
#if CURL_AT_LEAST_VERSION(7, 69, 0)
	mod->addNativeVar("OPT_MAIL_RCPT_ALLLOWFAILS",
			  vm.makeVar<VarInt>(loc, CURLOPT_MAIL_RCPT_ALLLOWFAILS));
#endif

	// TFTP OPTIONS
	mod->addNativeVar("OPT_TFTP_BLKSIZE", vm.makeVar<VarInt>(loc, CURLOPT_TFTP_BLKSIZE));
#if CURL_AT_LEAST_VERSION(7, 48, 0)
	mod->addNativeVar("OPT_TFTP_NO_OPTIONS", vm.makeVar<VarInt>(loc, CURLOPT_TFTP_NO_OPTIONS));
#endif

	// FTP OPTIONS
	mod->addNativeVar("OPT_FTPPORT", vm.makeVar<VarInt>(loc, CURLOPT_FTPPORT));
	mod->addNativeVar("OPT_QUOTE", vm.makeVar<VarInt>(loc, CURLOPT_QUOTE));
	mod->addNativeVar("OPT_POSTQUOTE", vm.makeVar<VarInt>(loc, CURLOPT_POSTQUOTE));
	mod->addNativeVar("OPT_PREQUOTE", vm.makeVar<VarInt>(loc, CURLOPT_PREQUOTE));
	mod->addNativeVar("OPT_APPEND", vm.makeVar<VarInt>(loc, CURLOPT_APPEND));
	mod->addNativeVar("OPT_FTP_USE_EPRT", vm.makeVar<VarInt>(loc, CURLOPT_FTP_USE_EPRT));
	mod->addNativeVar("OPT_FTP_USE_EPSV", vm.makeVar<VarInt>(loc, CURLOPT_FTP_USE_EPSV));
	mod->addNativeVar("OPT_FTP_USE_PRET", vm.makeVar<VarInt>(loc, CURLOPT_FTP_USE_PRET));
	mod->addNativeVar("OPT_FTP_CREATE_MISSING_DIRS",
			  vm.makeVar<VarInt>(loc, CURLOPT_FTP_CREATE_MISSING_DIRS));
	mod->addNativeVar("OPT_FTP_RESPONSE_TIMEOUT",
			  vm.makeVar<VarInt>(loc, CURLOPT_FTP_RESPONSE_TIMEOUT));
	mod->addNativeVar("OPT_FTP_ALTERNATIVE_TO_USER",
			  vm.makeVar<VarInt>(loc, CURLOPT_FTP_ALTERNATIVE_TO_USER));
	mod->addNativeVar("OPT_FTP_SKIP_PASV_IP",
			  vm.makeVar<VarInt>(loc, CURLOPT_FTP_SKIP_PASV_IP));
	mod->addNativeVar("OPT_FTPSSLAUTH", vm.makeVar<VarInt>(loc, CURLOPT_FTPSSLAUTH));
	mod->addNativeVar("OPT_FTP_SSL_CCC", vm.makeVar<VarInt>(loc, CURLOPT_FTP_SSL_CCC));
	mod->addNativeVar("OPT_FTP_ACCOUNT", vm.makeVar<VarInt>(loc, CURLOPT_FTP_ACCOUNT));
	mod->addNativeVar("OPT_FTP_FILEMETHOD", vm.makeVar<VarInt>(loc, CURLOPT_FTP_FILEMETHOD));

	// RTSP OPTIONS
	mod->addNativeVar("OPT_RTSP_REQUEST", vm.makeVar<VarInt>(loc, CURLOPT_RTSP_REQUEST));
	mod->addNativeVar("OPT_RTSP_SESSION_ID", vm.makeVar<VarInt>(loc, CURLOPT_RTSP_SESSION_ID));
	mod->addNativeVar("OPT_RTSP_STREAM_URI", vm.makeVar<VarInt>(loc, CURLOPT_RTSP_STREAM_URI));
	mod->addNativeVar("OPT_RTSP_TRANSPORT", vm.makeVar<VarInt>(loc, CURLOPT_RTSP_TRANSPORT));
	mod->addNativeVar("OPT_RTSP_CLIENT_CSEQ",
			  vm.makeVar<VarInt>(loc, CURLOPT_RTSP_CLIENT_CSEQ));
	mod->addNativeVar("OPT_RTSP_SERVER_CSEQ",
			  vm.makeVar<VarInt>(loc, CURLOPT_RTSP_SERVER_CSEQ));

	// PROTOCOL OPTIONS
	mod->addNativeVar("OPT_TRANSFERTEXT", vm.makeVar<VarInt>(loc, CURLOPT_TRANSFERTEXT));
	mod->addNativeVar("OPT_PROXY_TRANSFER_MODE",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TRANSFER_MODE));
	mod->addNativeVar("OPT_CRLF", vm.makeVar<VarInt>(loc, CURLOPT_CRLF));
	mod->addNativeVar("OPT_RANGE", vm.makeVar<VarInt>(loc, CURLOPT_RANGE));
	mod->addNativeVar("OPT_RESUME_FROM", vm.makeVar<VarInt>(loc, CURLOPT_RESUME_FROM));
	mod->addNativeVar("OPT_RESUME_FROM_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_RESUME_FROM_LARGE));
#if CURL_AT_LEAST_VERSION(7, 63, 0)
	mod->addNativeVar("OPT_CURLU", vm.makeVar<VarInt>(loc, CURLOPT_CURLU));
#endif
	mod->addNativeVar("OPT_CUSTOMREQUEST", vm.makeVar<VarInt>(loc, CURLOPT_CUSTOMREQUEST));
	mod->addNativeVar("OPT_FILETIME", vm.makeVar<VarInt>(loc, CURLOPT_FILETIME));
	mod->addNativeVar("OPT_DIRLISTONLY", vm.makeVar<VarInt>(loc, CURLOPT_DIRLISTONLY));
	mod->addNativeVar("OPT_NOBODY", vm.makeVar<VarInt>(loc, CURLOPT_NOBODY));
	mod->addNativeVar("OPT_INFILESIZE", vm.makeVar<VarInt>(loc, CURLOPT_INFILESIZE));
	mod->addNativeVar("OPT_INFILESIZE_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_INFILESIZE_LARGE));
	mod->addNativeVar("OPT_UPLOAD", vm.makeVar<VarInt>(loc, CURLOPT_UPLOAD));
#if CURL_AT_LEAST_VERSION(7, 62, 0)
	mod->addNativeVar("OPT_UPLOAD_BUFFERSIZE",
			  vm.makeVar<VarInt>(loc, CURLOPT_UPLOAD_BUFFERSIZE));
#endif
#if CURL_AT_LEAST_VERSION(7, 56, 0)
	mod->addNativeVar("OPT_MIMEPOST", vm.makeVar<VarInt>(loc, CURLOPT_MIMEPOST));
#endif
	mod->addNativeVar("OPT_MAXFILESIZE", vm.makeVar<VarInt>(loc, CURLOPT_MAXFILESIZE));
	mod->addNativeVar("OPT_MAXFILESIZE_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_MAXFILESIZE_LARGE));
	mod->addNativeVar("OPT_TIMECONDITION", vm.makeVar<VarInt>(loc, CURLOPT_TIMECONDITION));
	mod->addNativeVar("OPT_TIMEVALUE", vm.makeVar<VarInt>(loc, CURLOPT_TIMEVALUE));
#if CURL_AT_LEAST_VERSION(7, 59, 0)
	mod->addNativeVar("OPT_TIMEVALUE_LARGE", vm.makeVar<VarInt>(loc, CURLOPT_TIMEVALUE_LARGE));
#endif

	// CONNECTION OPTIONS
	mod->addNativeVar("OPT_TIMEOUT", vm.makeVar<VarInt>(loc, CURLOPT_TIMEOUT));
	mod->addNativeVar("OPT_TIMEOUT_MS", vm.makeVar<VarInt>(loc, CURLOPT_TIMEOUT_MS));
	mod->addNativeVar("OPT_LOW_SPEED_LIMIT", vm.makeVar<VarInt>(loc, CURLOPT_LOW_SPEED_LIMIT));
	mod->addNativeVar("OPT_LOW_SPEED_TIME", vm.makeVar<VarInt>(loc, CURLOPT_LOW_SPEED_TIME));
	mod->addNativeVar("OPT_MAX_SEND_SPEED_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_MAX_SEND_SPEED_LARGE));
	mod->addNativeVar("OPT_MAX_RECV_SPEED_LARGE",
			  vm.makeVar<VarInt>(loc, CURLOPT_MAX_RECV_SPEED_LARGE));
	mod->addNativeVar("OPT_MAXCONNECTS", vm.makeVar<VarInt>(loc, CURLOPT_MAXCONNECTS));
	mod->addNativeVar("OPT_FRESH_CONNECT", vm.makeVar<VarInt>(loc, CURLOPT_FRESH_CONNECT));
	mod->addNativeVar("OPT_FORBID_REUSE", vm.makeVar<VarInt>(loc, CURLOPT_FORBID_REUSE));
#if CURL_AT_LEAST_VERSION(7, 65, 0)
	mod->addNativeVar("OPT_MAXAGE_CONN", vm.makeVar<VarInt>(loc, CURLOPT_MAXAGE_CONN));
#endif
	mod->addNativeVar("OPT_CONNECTTIMEOUT", vm.makeVar<VarInt>(loc, CURLOPT_CONNECTTIMEOUT));
	mod->addNativeVar("OPT_CONNECTTIMEOUT_MS",
			  vm.makeVar<VarInt>(loc, CURLOPT_CONNECTTIMEOUT_MS));
	mod->addNativeVar("OPT_IPRESOLVE", vm.makeVar<VarInt>(loc, CURLOPT_IPRESOLVE));
	mod->addNativeVar("OPT_CONNECT_ONLY", vm.makeVar<VarInt>(loc, CURLOPT_CONNECT_ONLY));
	mod->addNativeVar("OPT_USE_SSL", vm.makeVar<VarInt>(loc, CURLOPT_USE_SSL));
	mod->addNativeVar("OPT_RESOLVE", vm.makeVar<VarInt>(loc, CURLOPT_RESOLVE));
	mod->addNativeVar("OPT_DNS_INTERFACE", vm.makeVar<VarInt>(loc, CURLOPT_DNS_INTERFACE));
	mod->addNativeVar("OPT_DNS_LOCAL_IP4", vm.makeVar<VarInt>(loc, CURLOPT_DNS_LOCAL_IP4));
	mod->addNativeVar("OPT_DNS_LOCAL_IP6", vm.makeVar<VarInt>(loc, CURLOPT_DNS_LOCAL_IP6));
	mod->addNativeVar("OPT_DNS_SERVERS", vm.makeVar<VarInt>(loc, CURLOPT_DNS_SERVERS));
#if CURL_AT_LEAST_VERSION(7, 60, 0)
	mod->addNativeVar("OPT_DNS_SHUFFLE_ADDRESSES",
			  vm.makeVar<VarInt>(loc, CURLOPT_DNS_SHUFFLE_ADDRESSES));
#endif
	mod->addNativeVar("OPT_ACCEPTTIMEOUT_MS",
			  vm.makeVar<VarInt>(loc, CURLOPT_ACCEPTTIMEOUT_MS));
#if CURL_AT_LEAST_VERSION(7, 59, 0)
	mod->addNativeVar("OPT_HAPPY_EYEBALLS_TIMEOUT_MS",
			  vm.makeVar<VarInt>(loc, CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS));
#endif
#if CURL_AT_LEAST_VERSION(7, 62, 0)
	mod->addNativeVar("OPT_UPKEEP_INTERVAL_MS",
			  vm.makeVar<VarInt>(loc, CURLOPT_UPKEEP_INTERVAL_MS));
#endif

	// SSL and SECURITY OPTIONS
	mod->addNativeVar("OPT_SSLCERT", vm.makeVar<VarInt>(loc, CURLOPT_SSLCERT));
	mod->addNativeVar("OPT_SSLCERTTYPE", vm.makeVar<VarInt>(loc, CURLOPT_SSLCERTTYPE));
	mod->addNativeVar("OPT_SSLKEY", vm.makeVar<VarInt>(loc, CURLOPT_SSLKEY));
	mod->addNativeVar("OPT_SSLKEYTYPE", vm.makeVar<VarInt>(loc, CURLOPT_SSLKEYTYPE));
	mod->addNativeVar("OPT_KEYPASSWD", vm.makeVar<VarInt>(loc, CURLOPT_KEYPASSWD));
	mod->addNativeVar("OPT_SSL_ENABLE_ALPN", vm.makeVar<VarInt>(loc, CURLOPT_SSL_ENABLE_ALPN));
	mod->addNativeVar("OPT_SSLENGINE", vm.makeVar<VarInt>(loc, CURLOPT_SSLENGINE));
	mod->addNativeVar("OPT_SSLENGINE_DEFAULT",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSLENGINE_DEFAULT));
	mod->addNativeVar("OPT_SSL_FALSESTART", vm.makeVar<VarInt>(loc, CURLOPT_SSL_FALSESTART));
	mod->addNativeVar("OPT_SSLVERSION", vm.makeVar<VarInt>(loc, CURLOPT_SSLVERSION));
	mod->addNativeVar("OPT_SSL_VERIFYPEER", vm.makeVar<VarInt>(loc, CURLOPT_SSL_VERIFYPEER));
	mod->addNativeVar("OPT_SSL_VERIFYHOST", vm.makeVar<VarInt>(loc, CURLOPT_SSL_VERIFYHOST));
	mod->addNativeVar("OPT_SSL_VERIFYSTATUS",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSL_VERIFYSTATUS));
#if CURL_AT_LEAST_VERSION(7, 52, 0)
	mod->addNativeVar("OPT_PROXY_CAINFO", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_CAINFO));
	mod->addNativeVar("OPT_PROXY_CAPATH", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_CAPATH));
	mod->addNativeVar("OPT_PROXY_CRLFILE", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_CRLFILE));
	mod->addNativeVar("OPT_PROXY_KEYPASSWD", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_KEYPASSWD));
	mod->addNativeVar("OPT_PROXY_PINNEDPUBLICKEY",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_PINNEDPUBLICKEY));
	mod->addNativeVar("OPT_PROXY_SSLCERT", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLCERT));
	mod->addNativeVar("OPT_PROXY_SSLCERTTYPE",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLCERTTYPE));
	mod->addNativeVar("OPT_PROXY_SSLKEY", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLKEY));
	mod->addNativeVar("OPT_PROXY_SSLKEYTYPE",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLKEYTYPE));
	mod->addNativeVar("OPT_PROXY_SSLVERSION",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLVERSION));
	mod->addNativeVar("OPT_PROXY_SSL_CIPHER_LIST",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_CIPHER_LIST));
	mod->addNativeVar("OPT_PROXY_SSL_OPTIONS",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_OPTIONS));
	mod->addNativeVar("OPT_PROXY_SSL_VERIFYHOST",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_VERIFYHOST));
	mod->addNativeVar("OPT_PROXY_SSL_VERIFYPEER",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_VERIFYPEER));
#endif
	mod->addNativeVar("OPT_CAINFO", vm.makeVar<VarInt>(loc, CURLOPT_CAINFO));
	mod->addNativeVar("OPT_ISSUERCERT", vm.makeVar<VarInt>(loc, CURLOPT_ISSUERCERT));
	mod->addNativeVar("OPT_CAPATH", vm.makeVar<VarInt>(loc, CURLOPT_CAPATH));
	mod->addNativeVar("OPT_CRLFILE", vm.makeVar<VarInt>(loc, CURLOPT_CRLFILE));
	mod->addNativeVar("OPT_CERTINFO", vm.makeVar<VarInt>(loc, CURLOPT_CERTINFO));
	mod->addNativeVar("OPT_PINNEDPUBLICKEY", vm.makeVar<VarInt>(loc, CURLOPT_PINNEDPUBLICKEY));
	mod->addNativeVar("OPT_SSL_CIPHER_LIST", vm.makeVar<VarInt>(loc, CURLOPT_SSL_CIPHER_LIST));
#if CURL_AT_LEAST_VERSION(7, 61, 0)
	mod->addNativeVar("OPT_TLS13_CIPHERS", vm.makeVar<VarInt>(loc, CURLOPT_TLS13_CIPHERS));
	mod->addNativeVar("OPT_PROXY_TLS13_CIPHERS",
			  vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLS13_CIPHERS));
#endif
	mod->addNativeVar("OPT_SSL_SESSIONID_CACHE",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSL_SESSIONID_CACHE));
	mod->addNativeVar("OPT_SSL_OPTIONS", vm.makeVar<VarInt>(loc, CURLOPT_SSL_OPTIONS));
	mod->addNativeVar("OPT_KRBLEVEL", vm.makeVar<VarInt>(loc, CURLOPT_KRBLEVEL));
	mod->addNativeVar("OPT_GSSAPI_DELEGATION",
			  vm.makeVar<VarInt>(loc, CURLOPT_GSSAPI_DELEGATION));

	// SSH OPTIONS
	mod->addNativeVar("OPT_SSH_AUTH_TYPES", vm.makeVar<VarInt>(loc, CURLOPT_SSH_AUTH_TYPES));
#if CURL_AT_LEAST_VERSION(7, 56, 0)
	mod->addNativeVar("OPT_SSH_COMPRESSION", vm.makeVar<VarInt>(loc, CURLOPT_SSH_COMPRESSION));
#endif
	mod->addNativeVar("OPT_SSH_HOST_PUBLIC_KEY_MD5",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSH_HOST_PUBLIC_KEY_MD5));
	mod->addNativeVar("OPT_SSH_PUBLIC_KEYFILE",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSH_PUBLIC_KEYFILE));
	mod->addNativeVar("OPT_SSH_PRIVATE_KEYFILE",
			  vm.makeVar<VarInt>(loc, CURLOPT_SSH_PRIVATE_KEYFILE));
	mod->addNativeVar("OPT_SSH_KNOWNHOSTS", vm.makeVar<VarInt>(loc, CURLOPT_SSH_KNOWNHOSTS));
	mod->addNativeVar("OPT_SSH_KEYFUNCTION", vm.makeVar<VarInt>(loc, CURLOPT_SSH_KEYFUNCTION));
	mod->addNativeVar("OPT_SSH_KEYDATA", vm.makeVar<VarInt>(loc, CURLOPT_SSH_KEYDATA));

	// OTHER OPTIONS
	mod->addNativeVar("OPT_PRIVATE", vm.makeVar<VarInt>(loc, CURLOPT_PRIVATE));
	mod->addNativeVar("OPT_SHARE", vm.makeVar<VarInt>(loc, CURLOPT_SHARE));
	mod->addNativeVar("OPT_NEW_FILE_PERMS", vm.makeVar<VarInt>(loc, CURLOPT_NEW_FILE_PERMS));
	mod->addNativeVar("OPT_NEW_DIRECTORY_PERMS",
			  vm.makeVar<VarInt>(loc, CURLOPT_NEW_DIRECTORY_PERMS));

	// TELNET OPTIONS
	mod->addNativeVar("OPT_TELNETOPTIONS", vm.makeVar<VarInt>(loc, CURLOPT_TELNETOPTIONS));

	// CURLINFO

	mod->addNativeVar("INFO_EFFECTIVE_URL", vm.makeVar<VarInt>(loc, CURLINFO_EFFECTIVE_URL));
	mod->addNativeVar("INFO_RESPONSE_CODE", vm.makeVar<VarInt>(loc, CURLINFO_RESPONSE_CODE));
	mod->addNativeVar("INFO_TOTAL_TIME", vm.makeVar<VarInt>(loc, CURLINFO_TOTAL_TIME));
	mod->addNativeVar("INFO_NAMELOOKUP_TIME",
			  vm.makeVar<VarInt>(loc, CURLINFO_NAMELOOKUP_TIME));
	mod->addNativeVar("INFO_CONNECT_TIME", vm.makeVar<VarInt>(loc, CURLINFO_CONNECT_TIME));
	mod->addNativeVar("INFO_PRETRANSFER_TIME",
			  vm.makeVar<VarInt>(loc, CURLINFO_PRETRANSFER_TIME));
	mod->addNativeVar("INFO_SIZE_UPLOAD_T", vm.makeVar<VarInt>(loc, CURLINFO_SIZE_UPLOAD_T));
	mod->addNativeVar("INFO_SIZE_DOWNLOAD_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_SIZE_DOWNLOAD_T));
	mod->addNativeVar("INFO_SPEED_DOWNLOAD_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_SPEED_DOWNLOAD_T));
	mod->addNativeVar("INFO_SPEED_UPLOAD_T", vm.makeVar<VarInt>(loc, CURLINFO_SPEED_UPLOAD_T));
	mod->addNativeVar("INFO_HEADER_SIZE", vm.makeVar<VarInt>(loc, CURLINFO_HEADER_SIZE));
	mod->addNativeVar("INFO_REQUEST_SIZE", vm.makeVar<VarInt>(loc, CURLINFO_REQUEST_SIZE));
	mod->addNativeVar("INFO_SSL_VERIFYRESULT",
			  vm.makeVar<VarInt>(loc, CURLINFO_SSL_VERIFYRESULT));
	mod->addNativeVar("INFO_FILETIME", vm.makeVar<VarInt>(loc, CURLINFO_FILETIME));
	mod->addNativeVar("INFO_FILETIME_T", vm.makeVar<VarInt>(loc, CURLINFO_FILETIME_T));
	mod->addNativeVar("INFO_CONTENT_LENGTH_DOWNLOAD_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T));
	mod->addNativeVar("INFO_CONTENT_LENGTH_UPLOAD_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_CONTENT_LENGTH_UPLOAD_T));
	mod->addNativeVar("INFO_STARTTRANSFER_TIME",
			  vm.makeVar<VarInt>(loc, CURLINFO_STARTTRANSFER_TIME));
	mod->addNativeVar("INFO_CONTENT_TYPE", vm.makeVar<VarInt>(loc, CURLINFO_CONTENT_TYPE));
	mod->addNativeVar("INFO_REDIRECT_TIME", vm.makeVar<VarInt>(loc, CURLINFO_REDIRECT_TIME));
	mod->addNativeVar("INFO_REDIRECT_COUNT", vm.makeVar<VarInt>(loc, CURLINFO_REDIRECT_COUNT));
	mod->addNativeVar("INFO_PRIVATE", vm.makeVar<VarInt>(loc, CURLINFO_PRIVATE));
	mod->addNativeVar("INFO_HTTP_CONNECTCODE",
			  vm.makeVar<VarInt>(loc, CURLINFO_HTTP_CONNECTCODE));
	mod->addNativeVar("INFO_HTTPAUTH_AVAIL", vm.makeVar<VarInt>(loc, CURLINFO_HTTPAUTH_AVAIL));
	mod->addNativeVar("INFO_PROXYAUTH_AVAIL",
			  vm.makeVar<VarInt>(loc, CURLINFO_PROXYAUTH_AVAIL));
	mod->addNativeVar("INFO_OS_ERRNO", vm.makeVar<VarInt>(loc, CURLINFO_OS_ERRNO));
	mod->addNativeVar("INFO_NUM_CONNECTS", vm.makeVar<VarInt>(loc, CURLINFO_NUM_CONNECTS));
	mod->addNativeVar("INFO_SSL_ENGINES", vm.makeVar<VarInt>(loc, CURLINFO_SSL_ENGINES));
	mod->addNativeVar("INFO_COOKIELIST", vm.makeVar<VarInt>(loc, CURLINFO_COOKIELIST));
	mod->addNativeVar("INFO_FTP_ENTRY_PATH", vm.makeVar<VarInt>(loc, CURLINFO_FTP_ENTRY_PATH));
	mod->addNativeVar("INFO_REDIRECT_URL", vm.makeVar<VarInt>(loc, CURLINFO_REDIRECT_URL));
	mod->addNativeVar("INFO_PRIMARY_IP", vm.makeVar<VarInt>(loc, CURLINFO_PRIMARY_IP));
	mod->addNativeVar("INFO_APPCONNECT_TIME",
			  vm.makeVar<VarInt>(loc, CURLINFO_APPCONNECT_TIME));
	mod->addNativeVar("INFO_CERTINFO", vm.makeVar<VarInt>(loc, CURLINFO_CERTINFO));
	mod->addNativeVar("INFO_CONDITION_UNMET",
			  vm.makeVar<VarInt>(loc, CURLINFO_CONDITION_UNMET));
	mod->addNativeVar("INFO_RTSP_SESSION_ID",
			  vm.makeVar<VarInt>(loc, CURLINFO_RTSP_SESSION_ID));
	mod->addNativeVar("INFO_RTSP_CLIENT_CSEQ",
			  vm.makeVar<VarInt>(loc, CURLINFO_RTSP_CLIENT_CSEQ));
	mod->addNativeVar("INFO_RTSP_SERVER_CSEQ",
			  vm.makeVar<VarInt>(loc, CURLINFO_RTSP_SERVER_CSEQ));
	mod->addNativeVar("INFO_RTSP_CSEQ_RECV", vm.makeVar<VarInt>(loc, CURLINFO_RTSP_CSEQ_RECV));
	mod->addNativeVar("INFO_PRIMARY_PORT", vm.makeVar<VarInt>(loc, CURLINFO_PRIMARY_PORT));
	mod->addNativeVar("INFO_LOCAL_IP", vm.makeVar<VarInt>(loc, CURLINFO_LOCAL_IP));
	mod->addNativeVar("INFO_LOCAL_PORT ", vm.makeVar<VarInt>(loc, CURLINFO_LOCAL_PORT));
	mod->addNativeVar("INFO_ACTIVESOCKET", vm.makeVar<VarInt>(loc, CURLINFO_ACTIVESOCKET));
	mod->addNativeVar("INFO_TLS_SSL_PTR", vm.makeVar<VarInt>(loc, CURLINFO_TLS_SSL_PTR));
	mod->addNativeVar("INFO_HTTP_VERSION", vm.makeVar<VarInt>(loc, CURLINFO_HTTP_VERSION));
	mod->addNativeVar("INFO_PROXY_SSL_VERIFYRESULT ",
			  vm.makeVar<VarInt>(loc, CURLINFO_PROXY_SSL_VERIFYRESULT));
	mod->addNativeVar("INFO_SCHEME", vm.makeVar<VarInt>(loc, CURLINFO_SCHEME));
	mod->addNativeVar("INFO_TOTAL_TIME_T", vm.makeVar<VarInt>(loc, CURLINFO_TOTAL_TIME_T));
	mod->addNativeVar("INFO_NAMELOOKUP_TIME_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_NAMELOOKUP_TIME_T));
	mod->addNativeVar("INFO_CONNECT_TIME_T", vm.makeVar<VarInt>(loc, CURLINFO_CONNECT_TIME_T));
	mod->addNativeVar("INFO_PRETRANSFER_TIME_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_PRETRANSFER_TIME_T));
	mod->addNativeVar("INFO_STARTTRANSFER_TIME_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_STARTTRANSFER_TIME_T));
	mod->addNativeVar("INFO_REDIRECT_TIME_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_REDIRECT_TIME_T));
	mod->addNativeVar("INFO_APPCONNECT_TIME_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_APPCONNECT_TIME_T));
	mod->addNativeVar("INFO_RETRY_AFTER", vm.makeVar<VarInt>(loc, CURLINFO_RETRY_AFTER));
	mod->addNativeVar("INFO_EFFECTIVE_METHOD",
			  vm.makeVar<VarInt>(loc, CURLINFO_EFFECTIVE_METHOD));
	mod->addNativeVar("INFO_PROXY_ERROR", vm.makeVar<VarInt>(loc, CURLINFO_PROXY_ERROR));
	mod->addNativeVar("INFO_REFERER", vm.makeVar<VarInt>(loc, CURLINFO_REFERER));
	mod->addNativeVar("INFO_CAINFO", vm.makeVar<VarInt>(loc, CURLINFO_CAINFO));
	mod->addNativeVar("INFO_CAPATH", vm.makeVar<VarInt>(loc, CURLINFO_CAPATH));
	mod->addNativeVar("INFO_XFER_ID", vm.makeVar<VarInt>(loc, CURLINFO_XFER_ID));
	mod->addNativeVar("INFO_CONN_ID", vm.makeVar<VarInt>(loc, CURLINFO_CONN_ID));
	mod->addNativeVar("INFO_QUEUE_TIME_T", vm.makeVar<VarInt>(loc, CURLINFO_QUEUE_TIME_T));
	mod->addNativeVar("INFO_USED_PROXY", vm.makeVar<VarInt>(loc, CURLINFO_USED_PROXY));
	mod->addNativeVar("INFO_POSTTRANSFER_TIME_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_POSTTRANSFER_TIME_T));
	mod->addNativeVar("INFO_EARLYDATA_SENT_T",
			  vm.makeVar<VarInt>(loc, CURLINFO_EARLYDATA_SENT_T));
	mod->addNativeVar("INFO_HTTPAUTH_USED", vm.makeVar<VarInt>(loc, CURLINFO_HTTPAUTH_USED));
	mod->addNativeVar("INFO_PROXYAUTH_USED", vm.makeVar<VarInt>(loc, CURLINFO_PROXYAUTH_USED));
	mod->addNativeVar("INFO_LASTONE", vm.makeVar<VarInt>(loc, CURLINFO_LASTONE));
}

} // namespace fer