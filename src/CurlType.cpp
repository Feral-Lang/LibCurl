#include "CurlType.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// VAR_CURL /////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurl::VarCurl(const ModuleLoc *loc, CURL *const val, bool owner)
	: Var(loc, typeID<VarCurl>(), false, false), val(val), owner(owner)
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
