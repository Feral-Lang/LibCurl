#include <std/FSType.hpp>

#include "CurlBase.hpp"
#include "CurlType.hpp"

Var *progressCallback = nullptr;
Vector<curl_slist *> hss;

size_t progressFuncIntervalTickMax = 10;

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Functions ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

int curlProgressFunc(void *ptr, curl_off_t to_download, curl_off_t downloaded, curl_off_t to_upload,
		     curl_off_t uploaded)
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

	static VarFlt var_to_download(nullptr, 0.0), var_downloaded(nullptr, 0.0);
	static VarFlt var_to_upload(nullptr, 0.0), var_uploaded(nullptr, 0.0);

	CurlVMData *vmd = (CurlVMData *)ptr;

	var_to_download.setLoc(vmd->loc);
	mpfr_set_si(var_to_download.get(), to_download, mpfr_get_default_rounding_mode());
	var_downloaded.setLoc(vmd->loc);
	mpfr_set_si(var_downloaded.get(), downloaded, mpfr_get_default_rounding_mode());
	var_to_upload.setLoc(vmd->loc);
	mpfr_set_si(var_to_upload.get(), to_upload, mpfr_get_default_rounding_mode());
	var_uploaded.setLoc(vmd->loc);
	mpfr_set_si(var_uploaded.get(), uploaded, mpfr_get_default_rounding_mode());
	Array<Var *, 5> args = {nullptr, &var_to_download, &var_downloaded, &var_to_upload,
				&var_uploaded};
	if(!progressCallback->call(*vmd->vm, vmd->loc, args, {})) {
		vmd->vm->fail(vmd->loc, "failed to call progress callback, check error above");
		return 1;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// CURL functions
///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Var *feralCurlEasySetOptNative(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			       const Map<String, AssnArgData> &assn_args)
{
	CURL *curl = as<VarCurl>(args[1])->get();
	if(!args[2]->is<VarInt>()) {
		vm.fail(loc, "expected an integer as parameter for option type, found: ",
			vm.getTypeName(args[2]));
		return nullptr;
	}
	int opt	 = mpz_get_si(as<VarInt>(args[2])->get());
	Var *arg = args[3];
	// for updating callbacks without much code repetition
	Var **callback		  = nullptr;
	size_t callback_arg_count = 0;

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
		res = curl_easy_setopt(curl, (CURLoption)opt, mpz_get_si(as<VarInt>(arg)->get()));
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
		res = curl_easy_setopt(curl, (CURLoption)opt, as<VarStr>(arg)->get().c_str());
		break;
	}
	case CURLOPT_XFERINFOFUNCTION: {
		callback	   = &progressCallback;
		callback_arg_count = 4;
		if(!arg->is<VarNil>()) {
			if(*callback) decref(*callback);
			*callback = nullptr;
			break;
		}
		if(!arg->isCallable()) {
			vm.fail(loc, "expected a callable as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		VarFn *f = as<VarFn>(arg);
		if(f->getParams().size() + f->getAssnParam().size() < callback_arg_count) {
			vm.fail(loc, "expected function to have ", callback_arg_count,
				" parameters for this option, found: ",
				f->getParams().size() + f->getAssnParam().size());
			return nullptr;
		}
		if(*callback) decref(*callback);
		incref(arg);
		*callback = f;
		break;
	}
	case CURLOPT_WRITEDATA: {
		if(!arg->is<VarFile>()) {
			vm.fail(loc, "expected a file as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		FILE *file     = as<VarFile>(arg)->getFile();
		StringRef mode = as<VarFile>(arg)->getMode();
		if(mode.find('w') == std::string::npos && mode.find('a') == std::string::npos) {
			vm.fail(loc, "file is not writable, opened mode: ", mode);
			return nullptr;
		}
		if(!file) {
			vm.fail(loc, "given file is not open");
			return nullptr;
		}
		res = curl_easy_setopt(curl, (CURLoption)opt, file);
		break;
	}
	case CURLOPT_HTTPHEADER: {
		if(!arg->is<VarVec>()) {
			vm.fail(
			loc, "expected a vector of strings as parameter for this option, found: ",
			vm.getTypeName(arg));
			return nullptr;
		}
		Vector<Var *> &vec = as<VarVec>(arg)->get();
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
			String &s = as<VarStr>(e)->get();
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

Var *feralCurlSetProgressFunc(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			      const Map<String, AssnArgData> &assn_args)
{
	Var *arg = args[1];
	if(!arg->isCallable()) {
		vm.fail(
		loc,
		"expected a callable as parameter for setting default progress function, found: ",
		vm.getTypeName(arg));
		return nullptr;
	}
	if(progressCallback) decref(progressCallback);
	incref(arg);
	progressCallback = as<VarFn>(arg);
	return vm.getNil();
}

Var *feralCurlSetProgressFuncTick(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
				  const Map<String, AssnArgData> &assn_args)
{
	Var *arg = args[1];
	if(!arg->is<VarInt>()) {
		vm.fail(loc,
			"expected an integer as parameter for setting default progress function "
			"tick interval, found: ",
			vm.getTypeName(arg));
		return nullptr;
	}
	progressFuncIntervalTickMax = mpz_get_ui(as<VarInt>(arg)->get());
	return vm.getNil();
}