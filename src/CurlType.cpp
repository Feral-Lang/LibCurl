#include "CurlType.hpp"

namespace fer
{

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// VARCURL //////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurl::VarCurl(ModuleLoc loc, CURL *const val, bool owner)
	: Var(loc, false, false), val(val), owner(owner)
{}
VarCurl::~VarCurl()
{
	if(owner && val) curl_easy_cleanup(val);
}

Var *VarCurl::onCopy(MemoryManager &mem, ModuleLoc loc)
{
	return Var::makeVarWithRef<VarCurl>(mem, loc, val, false);
}
void VarCurl::onSet(MemoryManager &mem, Var *from)
{
	if(owner && val) curl_easy_cleanup(val);
	owner = false;
	val   = as<VarCurl>(from)->getVal();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// VARCURLMIMEPART /////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurlMimePart::VarCurlMimePart(ModuleLoc loc, curl_mimepart *const val)
	: Var(loc, false, false), val(val)
{}
VarCurlMimePart::~VarCurlMimePart() {}

Var *VarCurlMimePart::onCopy(MemoryManager &mem, ModuleLoc loc)
{
	return Var::makeVarWithRef<VarCurlMimePart>(mem, loc, val);
}
void VarCurlMimePart::onSet(MemoryManager &mem, Var *from)
{
	val = as<VarCurlMimePart>(from)->getVal();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// VARCURLMIME //////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurlMime::VarCurlMime(ModuleLoc loc, curl_mime *const val, bool owner)
	: Var(loc, false, false), val(val), owner(owner)
{}
VarCurlMime::~VarCurlMime()
{
	if(owner && val) curl_mime_free(val);
}

Var *VarCurlMime::onCopy(MemoryManager &mem, ModuleLoc loc)
{
	return Var::makeVarWithRef<VarCurlMime>(mem, loc, val, false);
}
void VarCurlMime::onSet(MemoryManager &mem, Var *from)
{
	if(owner && val) curl_mime_free(val);
	owner = false;
	val   = as<VarCurlMime>(from)->getVal();
}

} // namespace fer