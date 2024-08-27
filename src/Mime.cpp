#include "CurlBase.hpp"
#include "CurlType.hpp"

namespace fer
{

Var *feralCurlMimeNew(Interpreter &vm, ModuleLoc loc, Span<Var *> args,
		      const StringMap<AssnArgData> &assn_args)
{
	return vm.makeVar<VarCurlMime>(loc, curl_mime_init(as<VarCurl>(args[0])->getVal()));
}

Var *feralCurlMimePartAddData(Interpreter &vm, ModuleLoc loc, Span<Var *> args,
			      const StringMap<AssnArgData> &assn_args)
{
	if(!args[1]->is<VarStr>()) {
		vm.fail(loc, "expected part name to be a string");
		return nullptr;
	}
	if(!args[2]->is<VarStr>()) {
		vm.fail(loc, "expected part data to be a string");
		return nullptr;
	}
	VarCurlMimePart *res =
	vm.makeVar<VarCurlMimePart>(loc, as<VarCurlMime>(args[0])->addPart());
	curl_mime_name(res->getVal(), as<VarStr>(args[1])->getVal().c_str());
	curl_mime_data(res->getVal(), as<VarStr>(args[2])->getVal().c_str(), CURL_ZERO_TERMINATED);
	return res;
}

Var *feralCurlMimePartAddFile(Interpreter &vm, ModuleLoc loc, Span<Var *> args,
			      const StringMap<AssnArgData> &assn_args)
{
	if(!args[1]->is<VarStr>()) {
		vm.fail(loc, "expected part name to be a string");
		return nullptr;
	}
	if(!args[2]->is<VarStr>()) {
		vm.fail(loc, "expected part filedata to be a string");
		return nullptr;
	}
	VarCurlMimePart *res =
	vm.makeVar<VarCurlMimePart>(loc, as<VarCurlMime>(args[0])->addPart());
	curl_mime_filedata(res->getVal(), as<VarStr>(args[2])->getVal().c_str());
	return res;
}

} // namespace fer