#pragma once

#include <curl/curl.h>
#include <VM/Interpreter.hpp>

using namespace fer;

// for passing to various callbacks
struct CurlVMData
{
	Interpreter *vm;
	const ModuleLoc *loc;
};

extern Var *progressCallback;
extern Vector<curl_slist *> hss;

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Functions ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

int curlProgressFunc(void *ptr, curl_off_t to_download, curl_off_t downloaded, curl_off_t to_upload,
		     curl_off_t uploaded);

Var *feralCurlEasySetOptNative(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			       const Map<String, AssnArgData> &assn_args);

// set some default values
Var *feralCurlSetProgressFunc(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			      const Map<String, AssnArgData> &assn_args);
Var *feralCurlSetProgressFuncTick(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
				  const Map<String, AssnArgData> &assn_args);
