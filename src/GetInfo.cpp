#include "CurlBase.hpp"
#include "CurlType.hpp"

namespace fer
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// CURL functions ////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

Var *feralCurlEasyGetInfoNative(VirtualMachine &vm, ModuleLoc loc, Span<Var *> args,
				const StringMap<AssnArgData> &assn_args)
{
	CURL *curl = as<VarCurl>(args[0])->getVal();
	if(!args[1]->is<VarInt>()) {
		vm.fail(loc, "expected an integer as parameter for option type, found: ",
			vm.getTypeName(args[1]));
		return nullptr;
	}
	int opt	 = as<VarInt>(args[1])->getVal();
	Var *arg = args[2];

	int res = CURLE_OK;
	// manually handle each of the options and work accordingly
	switch(opt) {
	case CURLINFO_ACTIVESOCKET: {
		if(!arg->is<VarInt>()) {
			vm.fail(loc, "expected an integer as parameter for this option, found: ",
				vm.getTypeName(arg));
			return nullptr;
		}
		long sockfd;
		res = curl_easy_getinfo(curl, (CURLINFO)opt, &sockfd);
		as<VarInt>(arg)->setVal(sockfd);
		break;
	}
	default: {
		vm.fail(loc, "operation is not yet implemented");
		return nullptr;
	}
	}
	return vm.makeVar<VarInt>(loc, res);
}

} // namespace fer