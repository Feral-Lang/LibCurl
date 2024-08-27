#include "CurlBase.hpp"
#include "CurlType.hpp"

namespace fer
{

Interpreter *cbVM = nullptr;

Var *progressCallback = nullptr;
Var *writeCallback    = nullptr;

Vector<curl_slist *> hss;

size_t progressFuncIntervalTickMax = 10;

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Functions ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

int curlProgressCallback(void *ptr, curl_off_t to_download, curl_off_t downloaded,
			 curl_off_t to_upload, curl_off_t uploaded)
{
	// ensure that the file to be downloaded is not empty
	// because that would cause a division by zero error later on
	if(to_download <= 0 && to_upload <= 0) return 0;

	if(!progressCallback) return 0;

	static size_t interval_tick = 0;
	if(interval_tick < progressFuncIntervalTickMax) {
		++interval_tick;
		return 0;
	}

	interval_tick = 0;

	static VarFlt var_to_download({}, 0.0), var_downloaded({}, 0.0);
	static VarFlt var_to_upload({}, 0.0), var_uploaded({}, 0.0);
	static Array<Var *, 5> args = {nullptr, &var_to_download, &var_downloaded, &var_to_upload,
				       &var_uploaded};

	ModuleLoc loc = *(ModuleLoc *)ptr;

	var_to_download.setLoc(loc);
	var_to_download.setVal(to_download);
	var_downloaded.setLoc(loc);
	var_downloaded.setVal(downloaded);
	var_to_upload.setLoc(loc);
	var_to_upload.setVal(to_upload);
	var_uploaded.setLoc(loc);
	var_uploaded.setVal(uploaded);
	if(!progressCallback->call(*cbVM, loc, args, {})) {
		cbVM->fail(loc, "failed to call progress callback, check error above");
		return 1;
	}
	return 0;
}

size_t curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	if(!writeCallback || !userdata) return 0;
	VarStr *dest = (VarStr *)userdata;

	static VarStr src({}, "");
	static Array<Var *, 3> args = {nullptr, dest, &src};
	src.setLoc(dest->getLoc());
	src.setVal(StringRef(ptr, size * nmemb));
	if(!writeCallback->call(*cbVM, dest->getLoc(), args, {})) {
		cbVM->fail(dest->getLoc(), "failed to call write callback, check error above");
		return 0;
	}
	return size * nmemb;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// CURL functions ////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

Var *feralCurlEasySetOptNative(Interpreter &vm, ModuleLoc loc, Span<Var *> args,
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
	case CURLOPT_FOLLOWLOCATION: // fallthrough
	case CURLOPT_NOPROGRESS: {
		if(!arg->is<VarInt>()) {
			vm.fail(loc, "expected an integer as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		res = curl_easy_setopt(curl, (CURLoption)opt, as<VarInt>(arg)->getVal());
		break;
	}
	case CURLOPT_URL:
	case CURLOPT_USERAGENT:
	case CURLOPT_CUSTOMREQUEST:
	case CURLOPT_POSTFIELDS: {
		if(!arg->is<VarStr>()) {
			vm.fail(loc, "expected a string as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		// tmp shenanigans because curl does not copy the string for POSTFIELDS in an
		// internal buffer
		static String tmp;
		tmp.clear();
		tmp = as<VarStr>(arg)->getVal();
		res = curl_easy_setopt(curl, (CURLoption)opt, tmp.c_str());
		break;
	}
	case CURLOPT_MIMEPOST: {
		if(!arg->is<VarCurlMime>()) {
			vm.fail(loc, "expected curl mime as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		res = curl_easy_setopt(curl, (CURLoption)opt, as<VarCurlMime>(arg)->getVal());
		break;
	}
	case CURLOPT_XFERINFOFUNCTION: {
		if(arg->is<VarNil>()) {
			if(progressCallback) vm.decVarRef(progressCallback);
			progressCallback = nullptr;
			break;
		}
		if(!arg->isCallable()) {
			vm.fail(loc, "expected a callable as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		VarFn *f = as<VarFn>(arg);
		if(f->getParams().size() + f->getAssnParam().size() < 4) {
			vm.fail(loc, "expected function to have 4",
				" parameters for this option, found: ",
				f->getParams().size() + f->getAssnParam().size());
			return nullptr;
		}
		if(progressCallback) vm.decVarRef(progressCallback);
		vm.incVarRef(f);
		progressCallback = f;
		curl_easy_setopt(curl, (CURLoption)opt, curlProgressCallback);
		break;
	}
	case CURLOPT_WRITEFUNCTION: {
		if(arg->is<VarNil>()) {
			if(writeCallback) vm.decVarRef(writeCallback);
			writeCallback = nullptr;
			break;
		}
		if(!arg->isCallable()) {
			vm.fail(loc, "expected a callable as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		VarFn *f = as<VarFn>(arg);
		if(f->getParams().size() + f->getAssnParam().size() < 2) {
			vm.fail(loc, "expected function to have 2",
				" parameters for this option, found: ",
				f->getParams().size() + f->getAssnParam().size());
			return nullptr;
		}
		if(writeCallback) vm.decVarRef(writeCallback);
		vm.incVarRef(f);
		writeCallback = f;
		curl_easy_setopt(curl, (CURLoption)opt, curlWriteCallback);
		break;
	}
	case CURLOPT_WRITEDATA: {
		if(!arg->is<VarFile>() && !arg->is<VarStr>()) {
			vm.fail(loc,
				"expected a file or string as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		if(arg->is<VarFile>()) {
			FILE *file     = as<VarFile>(arg)->getFile();
			StringRef mode = as<VarFile>(arg)->getMode();
			if(mode.find('w') == std::string::npos &&
			   mode.find('a') == std::string::npos)
			{
				vm.fail(loc, "file is not writable, opened mode: ", mode);
				return nullptr;
			}
			if(!file) {
				vm.fail(loc, "given file is not open");
				return nullptr;
			}
			res = curl_easy_setopt(curl, (CURLoption)opt, file);
		} else if(arg->is<VarStr>()) {
			res = curl_easy_setopt(curl, (CURLoption)opt, arg);
		}
		break;
	}
	case CURLOPT_HTTPHEADER: {
		if(!arg->is<VarVec>()) {
			vm.fail(
			loc, "expected a vector of strings as parameter for this option, found: ",
			vm.getTypeName(arg));
			return nullptr;
		}
		Vector<Var *> &vec = as<VarVec>(arg)->getVal();
		for(size_t i = 0; i < vec.size(); ++i) {
			if(!vec[i]->is<VarStr>()) {
				vm.fail(loc,
					"expected a string vector, found: ", vm.getTypeName(vec[i]),
					" at index: ", i);
				return nullptr;
			}
		}
		struct curl_slist *hs = NULL;
		for(auto &e : vec) {
			String &s = as<VarStr>(e)->getVal();
			hs	  = curl_slist_append(hs, s.c_str());
		}
		res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
		hss.push_back(hs);
		break;
	}
	default: {
		vm.fail(loc, "operation is not yet implemented");
		return nullptr;
	}
	}
	return vm.makeVar<VarInt>(loc, res);
}

Var *feralCurlSetWriteCallback(Interpreter &vm, ModuleLoc loc, Span<Var *> args,
			       const StringMap<AssnArgData> &assn_args)
{
	Var *arg = args[1];
	if(!arg->isCallable()) {
		vm.fail(
		loc, "expected a callable as parameter for setting default write function, found: ",
		vm.getTypeName(arg));
		return nullptr;
	}
	if(writeCallback) vm.decVarRef(writeCallback);
	vm.incVarRef(arg);
	writeCallback = as<VarFn>(arg);
	return vm.getNil();
}

Var *feralCurlSetProgressCallback(Interpreter &vm, ModuleLoc loc, Span<Var *> args,
				  const StringMap<AssnArgData> &assn_args)
{
	Var *arg = args[1];
	if(!arg->isCallable()) {
		vm.fail(loc,
			"expected a callable as parameter for"
			" setting default progress function, found: ",
			vm.getTypeName(arg));
		return nullptr;
	}
	if(progressCallback) vm.decVarRef(progressCallback);
	vm.incVarRef(arg);
	progressCallback = as<VarFn>(arg);
	return vm.getNil();
}

Var *feralCurlSetProgressCallbackTick(Interpreter &vm, ModuleLoc loc, Span<Var *> args,
				      const StringMap<AssnArgData> &assn_args)
{
	Var *arg = args[1];
	if(!arg->is<VarInt>()) {
		vm.fail(loc,
			"expected an integer as parameter for setting default progress function "
			"tick interval, found: ",
			vm.getTypeName(arg));
		return nullptr;
	}
	progressFuncIntervalTickMax = as<VarInt>(arg)->getVal();
	return vm.getNil();
}

} // namespace fer