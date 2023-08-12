#include "CurlBase.hpp"
#include "CurlType.hpp"

Var *feralCurlMimeNew(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
		      const Map<String, AssnArgData> &assn_args)
{
	return vm.makeVar<VarCurlMime>(loc, curl_mime_init(as<VarCurl>(args[0])->get()));
}

Var *feralCurlMimePartAddData(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			      const Map<String, AssnArgData> &assn_args)
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
	curl_mime_name(res->get(), as<VarStr>(args[1])->get().c_str());
	curl_mime_data(res->get(), as<VarStr>(args[2])->get().c_str(), CURL_ZERO_TERMINATED);
	return res;
}

Var *feralCurlMimePartAddFile(Interpreter &vm, const ModuleLoc *loc, Span<Var *> args,
			      const Map<String, AssnArgData> &assn_args)
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
	curl_mime_filedata(res->get(), as<VarStr>(args[2])->get().c_str());
	return res;
}
