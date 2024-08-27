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

	Var *onCopy(Interpreter &vm, ModuleLoc loc) override;
	void onSet(Interpreter &vm, Var *from) override;

public:
	VarCurl(ModuleLoc loc, CURL *const val, bool owner = true);
	~VarCurl();

	inline CURL *const getVal() { return val; }
};

class VarCurlMimePart : public Var
{
	curl_mimepart *val;

	Var *onCopy(Interpreter &vm, ModuleLoc loc) override;
	void onSet(Interpreter &vm, Var *from) override;

public:
	VarCurlMimePart(ModuleLoc loc, curl_mimepart *const val);
	~VarCurlMimePart();

	inline curl_mimepart *const getVal() { return val; }
};

class VarCurlMime : public Var
{
	curl_mime *val;
	bool owner;

	Var *onCopy(Interpreter &vm, ModuleLoc loc) override;
	void onSet(Interpreter &vm, Var *from) override;

public:
	VarCurlMime(ModuleLoc loc, curl_mime *const val, bool owner = true);
	~VarCurlMime();

	inline curl_mime *const getVal() { return val; }
	inline curl_mimepart *addPart() { return curl_mime_addpart(val); }
};

} // namespace fer