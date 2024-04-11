#pragma once

#include <curl/curl.h>
#include <VM/Interpreter.hpp>

namespace fer
{

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// VARCURL //////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

class VarCurl : public Var
{
	CURL *val;
	bool owner;

public:
	VarCurl(const ModuleLoc *loc, CURL *const val, bool owner = true);
	~VarCurl();

	Var *copy(const ModuleLoc *loc);
	void set(Var *from);

	inline CURL *const get() { return val; }
};

class VarCurlMimePart : public Var
{
	curl_mimepart *val;

public:
	VarCurlMimePart(const ModuleLoc *loc, curl_mimepart *const val);
	~VarCurlMimePart();

	Var *copy(const ModuleLoc *loc);
	void set(Var *from);

	inline curl_mimepart *const get() { return val; }
};

class VarCurlMime : public Var
{
	curl_mime *val;
	bool owner;

public:
	VarCurlMime(const ModuleLoc *loc, curl_mime *const val, bool owner = true);
	~VarCurlMime();

	Var *copy(const ModuleLoc *loc);
	void set(Var *from);

	inline curl_mime *const get() { return val; }
	inline curl_mimepart *addPart() { return curl_mime_addpart(val); }
};

} // namespace fer