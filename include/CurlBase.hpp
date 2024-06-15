#pragma once

#include <curl/curl.h>
#include <VM/Interpreter.hpp>

namespace fer
{

// for use in callbacks
extern Interpreter *cbVM;

extern Var *progressCallback;
extern Var *writeCallback;

extern Vector<curl_slist *> hss;

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Functions ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

int curlProgressCallback(void *ptr, curl_off_t to_download, curl_off_t downloaded,
			 curl_off_t to_upload, curl_off_t uploaded);
size_t curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);

Var *feralCurlEasySetOptNative(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			       const StringMap<AssnArgData> &assn_args);

// set some default values
Var *feralCurlSetWriteCallback(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			       const StringMap<AssnArgData> &assn_args);
Var *feralCurlSetProgressCallback(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
				  const StringMap<AssnArgData> &assn_args);
Var *feralCurlSetProgressCallbackTick(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
				      const StringMap<AssnArgData> &assn_args);

// Mime Functions

Var *feralCurlMimeNew(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
		      const StringMap<AssnArgData> &assn_args);
Var *feralCurlMimePartAddData(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			      const StringMap<AssnArgData> &assn_args);
Var *feralCurlMimePartAddFile(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			      const StringMap<AssnArgData> &assn_args);

} // namespace fer