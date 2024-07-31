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

	Var *copyImpl(const ModuleLoc *loc) override;

public:
	VarCurl(const ModuleLoc *loc, CURL *const val, bool owner = true);
	~VarCurl();

	void set(Var *from) override;

	inline CURL *const get() { return val; }
};

class VarCurlMimePart : public Var
{
	curl_mimepart *val;

	Var *copyImpl(const ModuleLoc *loc) override;

public:
	VarCurlMimePart(const ModuleLoc *loc, curl_mimepart *const val);
	~VarCurlMimePart();

	void set(Var *from) override;

	inline curl_mimepart *const get() { return val; }
};

class VarCurlMime : public Var
{
	curl_mime *val;
	bool owner;

	Var *copyImpl(const ModuleLoc *loc) override;

public:
	VarCurlMime(const ModuleLoc *loc, curl_mime *const val, bool owner = true);
	~VarCurlMime();

	void set(Var *from) override;

	inline curl_mime *const get() { return val; }
	inline curl_mimepart *addPart() { return curl_mime_addpart(val); }
};

} // namespace fer