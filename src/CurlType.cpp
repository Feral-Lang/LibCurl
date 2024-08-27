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

Var *VarCurl::onCopy(Interpreter &vm, ModuleLoc loc)
{
	return vm.makeVarWithRef<VarCurl>(loc, val, false);
}
void VarCurl::onSet(Interpreter &vm, Var *from)
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

Var *VarCurlMimePart::onCopy(Interpreter &vm, ModuleLoc loc)
{
	return vm.makeVarWithRef<VarCurlMimePart>(loc, val);
}
void VarCurlMimePart::onSet(Interpreter &vm, Var *from)
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

Var *VarCurlMime::onCopy(Interpreter &vm, ModuleLoc loc)
{
	return vm.makeVarWithRef<VarCurlMime>(loc, val, false);
}
void VarCurlMime::onSet(Interpreter &vm, Var *from)
{
	if(owner && val) curl_mime_free(val);
	owner = false;
	val   = as<VarCurlMime>(from)->getVal();
}

} // namespace fer