#include "CurlType.hpp"

namespace fer
{

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// VARCURL //////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurl::VarCurl(const ModuleLoc *loc, CURL *const val, bool owner)
	: Var(loc, false, false), val(val), owner(owner)
{}
VarCurl::~VarCurl()
{
	if(owner && val) curl_easy_cleanup(val);
}

Var *VarCurl::copy(const ModuleLoc *loc) { return new VarCurl(loc, val, false); }

void VarCurl::set(Var *from)
{
	if(owner && val) curl_easy_cleanup(val);
	owner = false;
	val   = as<VarCurl>(from)->get();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// VARCURLMIMEPART /////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurlMimePart::VarCurlMimePart(const ModuleLoc *loc, curl_mimepart *const val)
	: Var(loc, false, false), val(val)
{}
VarCurlMimePart::~VarCurlMimePart() {}

Var *VarCurlMimePart::copy(const ModuleLoc *loc) { return new VarCurlMimePart(loc, val); }

void VarCurlMimePart::set(Var *from) { val = as<VarCurlMimePart>(from)->get(); }

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// VARCURLMIME //////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurlMime::VarCurlMime(const ModuleLoc *loc, curl_mime *const val, bool owner)
	: Var(loc, false, false), val(val), owner(owner)
{}
VarCurlMime::~VarCurlMime()
{
	if(owner && val) curl_mime_free(val);
}

Var *VarCurlMime::copy(const ModuleLoc *loc) { return new VarCurlMime(loc, val, false); }

void VarCurlMime::set(Var *from)
{
	if(owner && val) curl_mime_free(val);
	owner = false;
	val   = as<VarCurlMime>(from)->get();
}

} // namespace fer