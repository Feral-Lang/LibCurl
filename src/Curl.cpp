#include "Curl.hpp"

namespace fer
{

constexpr size_t CURL_DEFAULT_PROGRESS_INTERVAL_TICK_MAX = 10;

void setEnumVars(VirtualMachine &vm, VarModule *mod, ModuleLoc loc);

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Callbacks ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

int curlProgressCallback(void *ptr, curl_off_t dlTotal, curl_off_t dlDone, curl_off_t ulTotal,
                         curl_off_t ulDone)
{
    // ensure that the file to be downloaded is not empty
    // because that would cause a division by zero error later on
    if(dlTotal <= 0 && ulTotal <= 0) return 0;

    CurlCallbackData &cbdata = *(CurlCallbackData *)ptr;

    if(!cbdata.curl->getProgressCB()) return 0;

    size_t &intervalTick = cbdata.curl->getProgIntervalTick();
    if(intervalTick < cbdata.curl->getProgIntervalTickMax()) {
        ++intervalTick;
        return 0;
    }
    intervalTick = 0;

    VarVec *argsVar = cbdata.curl->getProgressCBArgs();
    as<VarFlt>(argsVar->at(1))->setVal(dlTotal);
    as<VarFlt>(argsVar->at(2))->setVal(dlDone);
    as<VarFlt>(argsVar->at(3))->setVal(ulTotal);
    as<VarFlt>(argsVar->at(4))->setVal(ulDone);
    if(!cbdata.curl->getProgressCB()->call(cbdata.vm, cbdata.loc, argsVar->getVal(), {})) {
        cbdata.vm.fail(cbdata.loc, "failed to call progress callback, check error above");
        return 1;
    }
    return 0;
}

size_t curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    CurlCallbackData &cbdata = *(CurlCallbackData *)userdata;
    if(!cbdata.curl->getWriteCB()) return size * nmemb; // returning zero is an error

    VarVec *argsVar = cbdata.curl->getWriteCBArgs();
    as<VarStr>(argsVar->at(1))->setVal(StringRef(ptr, size * nmemb));
    if(!cbdata.curl->getWriteCB()->call(cbdata.vm, cbdata.loc, argsVar->getVal(), {})) {
        cbdata.vm.fail(cbdata.loc, "failed to call write callback, check error above");
        return 0;
    }
    return size * nmemb;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// VarCurl //////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurl::VarCurl(ModuleLoc loc, CURL *val)
    : Var(loc, false, false), val(val), progCB(nullptr), writeCB(nullptr), progCBArgs(nullptr),
      writeCBArgs(nullptr), progIntervalTick(0),
      progIntervalTickMax(CURL_DEFAULT_PROGRESS_INTERVAL_TICK_MAX)
{}
VarCurl::~VarCurl()
{
    clearMimeData();
    curl_easy_cleanup(val);
}

void VarCurl::onCreate(MemoryManager &mem)
{
    progCBArgs = Var::makeVarWithRef<VarVec>(mem, getLoc(), 5, true);
    progCBArgs->push(nullptr);
    progCBArgs->push(Var::makeVarWithRef<VarFlt>(mem, getLoc(), 0.0));
    progCBArgs->push(Var::makeVarWithRef<VarFlt>(mem, getLoc(), 0.0));
    progCBArgs->push(Var::makeVarWithRef<VarFlt>(mem, getLoc(), 0.0));
    progCBArgs->push(Var::makeVarWithRef<VarFlt>(mem, getLoc(), 0.0));

    writeCBArgs = Var::makeVarWithRef<VarVec>(mem, ModuleLoc(), 2, true);
    writeCBArgs->push(nullptr);
    writeCBArgs->push(Var::makeVarWithRef<VarStr>(mem, getLoc(), ""));
}
void VarCurl::onDestroy(MemoryManager &mem)
{
    Var::decVarRef(mem, writeCBArgs);
    Var::decVarRef(mem, progCBArgs);
    setProgressCB(mem, nullptr, {});
    setWriteCB(mem, nullptr, {});
}

void VarCurl::setProgressCB(MemoryManager &mem, VarFn *_progCB, Span<Var *> args)
{
    if(progCB) Var::decVarRef(mem, progCB);
    progCB = _progCB;
    if(progCB) Var::incVarRef(progCB);
    if(!progCBArgs) return;
    while(progCBArgs->size() > 5) {
        Var::decVarRef(mem, progCBArgs->back());
        progCBArgs->pop();
    }
    for(auto &arg : args) {
        Var::incVarRef(arg);
        progCBArgs->push(arg);
    }
}
void VarCurl::setWriteCB(MemoryManager &mem, VarFn *_writeCB, Span<Var *> args)
{
    if(writeCB) Var::decVarRef(mem, writeCB);
    writeCB = _writeCB;
    if(writeCB) Var::incVarRef(writeCB);
    if(!writeCBArgs) return;
    while(writeCBArgs->size() > 5) {
        Var::decVarRef(mem, writeCBArgs->back());
        writeCBArgs->pop();
    }
    for(auto &arg : args) {
        Var::incVarRef(arg);
        writeCBArgs->push(arg);
    }
}

curl_mime *VarCurl::createMime(VirtualMachine &vm, ModuleLoc loc, Var *data)
{
    if(data->is<VarMap>() && as<VarMap>(data)->getVal().empty()) return nullptr;

    mimelist.push_front(curl_mime_init(val));
    curl_mime *mime = mimelist.front();
    if(data->is<VarStr>()) {
        curl_mimepart *part = curl_mime_addpart(mime);
        curl_mime_filedata(part, as<VarStr>(data)->getVal().c_str());
        curl_mime_filename(part, as<VarStr>(data)->getVal().c_str());
    } else {
        VarMap *map = as<VarMap>(data);
        for(auto &item : map->getVal()) {
            Var *v = nullptr;
            Array<Var *, 1> tmp{item.second};
            if(!vm.callVarAndExpect<VarStr>(loc, "str", v, tmp, {})) {
                curl_mime_free(mime);
                mimelist.pop_front();
                return nullptr;
            }
            const String &str   = as<VarStr>(v)->getVal();
            curl_mimepart *part = curl_mime_addpart(mime);
            curl_mime_name(part, item.first.c_str());
            curl_mime_data(part, str.c_str(), str.size());
            vm.decVarRef(v);
        }
    }
    return mime;
}
void VarCurl::clearMimeData()
{
    while(!mimelist.empty()) {
        curl_mime_free(mimelist.front());
        mimelist.pop_front();
    }
}

curl_slist *VarCurl::createSList(VirtualMachine &vm, ModuleLoc loc, Var *data)
{
    if(data->is<VarMap>() && as<VarMap>(data)->getVal().empty()) return nullptr;

    sllist.push_front(nullptr);
    curl_slist *&lst = sllist.front();
    if(data->is<VarStr>()) {
        lst = curl_slist_append(lst, as<VarStr>(data)->getVal().c_str());
    } else {
        String tmpStr;
        VarMap *map = as<VarMap>(data);
        for(auto &item : map->getVal()) {
            Var *v = nullptr;
            Array<Var *, 1> tmp{item.second};
            if(!vm.callVarAndExpect<VarStr>(loc, "str", v, tmp, {})) {
                curl_slist_free_all(lst);
                sllist.pop_front();
                return nullptr;
            }
            const String &str = as<VarStr>(v)->getVal();
            tmpStr.clear();
            tmpStr += item.first;
            tmpStr += ": ";
            tmpStr += str.c_str();
            lst = curl_slist_append(lst, tmpStr.c_str());
            vm.decVarRef(v);
        }
    }
    return lst;
}
void VarCurl::clearSList()
{
    while(!sllist.empty()) {
        curl_slist_free_all(sllist.front());
        sllist.pop_front();
    }
}

CurlCallbackData::CurlCallbackData(ModuleLoc loc, VirtualMachine &vm, VarCurl *curl)
    : loc(loc), vm(vm), curl(curl)
{}

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Functions ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

FERAL_FUNC(feralCurlGlobalTrace, 1, false,
           "  fn(trace) -> Int\n"
           "Sets the global `trace` config for the curl library.")
{
    EXPECT(VarStr, args[1], "trace config");
    int res = curl_global_trace(as<VarStr>(args[1])->getVal().c_str());
    return vm.makeVar<VarInt>(loc, res);
}

FERAL_FUNC(
    feralCurlEasyInit, 0, false,
    "  fn() -> Curl\n"
    "Creates and returns a Curl (Easy) instance which can be used to perform network operations.")
{
    CURL *curl = curl_easy_init();
    if(!curl) {
        vm.fail(loc, "failed to run curl_easy_init()");
        return nullptr;
    }
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgressCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    return vm.makeVar<VarCurl>(loc, curl);
}

FERAL_FUNC(feralCurlEasyPerform, 0, false,
           "  var.fn() -> Int\n"
           "Performs the required operations on the Curl object `var` and returns the status code "
           "of the finished operation.")
{
    CURL *curl = as<VarCurl>(args[0])->getVal();
    CurlCallbackData cbdata(loc, vm, as<VarCurl>(args[0]));
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cbdata);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cbdata);
    return vm.makeVar<VarInt>(loc, curl_easy_perform(curl));
}

FERAL_FUNC(feralCurlEasyStrErrFromInt, 1, false,
           "  fn(errCode) -> Str\n"
           "Returns the string representation of the error code `errCode`.")
{
    EXPECT(VarInt, args[1], "error code");
    CURLcode code = (CURLcode)as<VarInt>(args[1])->getVal();
    return vm.makeVar<VarStr>(loc, curl_easy_strerror(code));
}

FERAL_FUNC(feralCurlSetProgressCBTick, 1, false, "")
{
    EXPECT(VarInt, args[1], "tick count");
    VarCurl *curl = as<VarCurl>(args[0]);
    curl->setProgIntervalTickMax(as<VarInt>(args[1])->getVal());
    return vm.getNil();
}

FERAL_FUNC(feralCurlEasyGetInfoNative, 2, false,
           "  var.fn(option, suboption) -> Int\n"
           "Gets the info for the Curl `option` in the curl object `var`, possibly with a "
           "`suboption`, and returns it as an integer.")
{
    EXPECT(VarInt, args[1], "option type (CURL_OPT_*)");
    CURL *curl = as<VarCurl>(args[0])->getVal();
    int opt    = as<VarInt>(args[1])->getVal();
    Var *arg   = args[2];

    int res = CURLE_OK;
    // manually handle each of the options and work accordingly
    switch(opt) {
    case CURLINFO_ACTIVESOCKET: {
        EXPECT(VarInt, arg, "option value");
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

FERAL_FUNC(feralCurlEasySetOptNative, 2, true,
           "  var.fn(option, suboption/value...) -> Int\n"
           "Sets the `option` in Curl `var` as with one/more `suboption/value` and returns the "
           "integer result.\n"
           "Here, type and count of `suboption/value` are dependent on the `option` being used.")
{
    EXPECT(VarInt, args[1], "option type (CURL_OPT_*)");
    VarCurl *varCurl = as<VarCurl>(args[0]);
    CURL *curl       = varCurl->getVal();
    int opt          = as<VarInt>(args[1])->getVal();
    Var *arg         = args[2];

    int res = CURLE_OK;
    // manually handle each of the options and work accordingly
    switch(opt) {
    case CURLOPT_CONNECT_ONLY:   // fallthrough
    case CURLOPT_FOLLOWLOCATION: // fallthrough
    case CURLOPT_NOPROGRESS:     // fallthrough
    case CURLOPT_VERBOSE: {
        EXPECT(VarInt, arg, "option value");
        res = curl_easy_setopt(curl, (CURLoption)opt, as<VarInt>(arg)->getVal());
        break;
    }
    case CURLOPT_POSTFIELDS: {
        // We don't want POSTFIELDS as it doesn't copy the string data to curl,
        // which is annoying to deal with.
        opt = CURLOPT_COPYPOSTFIELDS;
    }
    case CURLOPT_URL:
    case CURLOPT_USERAGENT:
    case CURLOPT_CUSTOMREQUEST:
    case CURLOPT_COPYPOSTFIELDS: {
        EXPECT(VarStr, arg, "option value");
        res = curl_easy_setopt(curl, (CURLoption)opt, as<VarStr>(arg)->getVal().c_str());
        break;
    }
    case CURLOPT_MIMEPOST: {
        EXPECT(VarMap, arg, "name-data pairs");
        curl_mime *mime = varCurl->createMime(vm, loc, as<VarMap>(arg));
        if(!mime) {
            vm.fail(loc, "failed to create mime from the given map (possibly empty map)");
            return nullptr;
        }
        res = curl_easy_setopt(curl, (CURLoption)opt, mime);
        break;
    }
    case CURLOPT_XFERINFOFUNCTION: {
        if(arg->is<VarNil>()) {
            varCurl->setProgressCB(vm.getMemoryManager(), nullptr, {});
            break;
        }
        EXPECT(VarFn, arg, "xfer info function");
        VarFn *f = as<VarFn>(arg);
        if(f->getParams().size() + f->getAssnParam().size() < 4) {
            vm.fail(loc, "expected function to have at least 4",
                    " parameters for this option, found: ",
                    f->getParams().size() + f->getAssnParam().size());
            return nullptr;
        }
        Span<Var *> cbArgs{args.begin() + 3, args.end()};
        varCurl->setProgressCB(vm.getMemoryManager(), f, cbArgs);
        break;
    }
    case CURLOPT_WRITEFUNCTION: {
        if(arg->is<VarNil>()) {
            varCurl->setWriteCB(vm.getMemoryManager(), nullptr, {});
            break;
        }
        EXPECT(VarFn, arg, "write function");
        VarFn *f = as<VarFn>(arg);
        if(f->getParams().size() + f->getAssnParam().size() < 1) {
            vm.fail(loc, "expected function to have at least 1",
                    " parameter for this option, found: ",
                    f->getParams().size() + f->getAssnParam().size());
            return nullptr;
        }
        Span<Var *> cbArgs{args.begin() + 3, args.end()};
        varCurl->setWriteCB(vm.getMemoryManager(), f, cbArgs);
        break;
    }
    case CURLOPT_HTTPHEADER: {
        EXPECT(VarMap, arg, "name-data pairs");
        curl_slist *lst = varCurl->createSList(vm, loc, as<VarMap>(arg));
        if(!lst) {
            vm.fail(loc, "failed to create slist from the given map (possibly empty map)");
            return nullptr;
        }
        res = curl_easy_setopt(curl, (CURLoption)opt, lst);
        break;
    }
    default: {
        vm.fail(loc, "operation is not yet implemented");
        return nullptr;
    }
    }
    return vm.makeVar<VarInt>(loc, res);
}

INIT_MODULE(Curl)
{
    curl_global_init(CURL_GLOBAL_ALL);

    VarModule *mod = vm.getCurrModule();

    // Register the type names
    vm.registerType<VarCurl>(loc, "Curl", "The Curl C library's type representation.");

    mod->addNativeFn(vm, "globalTrace", feralCurlGlobalTrace);
    mod->addNativeFn(vm, "strerr", feralCurlEasyStrErrFromInt);
    mod->addNativeFn(vm, "newEasy", feralCurlEasyInit);

    vm.addNativeTypeFn<VarCurl>(loc, "getInfoNative", feralCurlEasyGetInfoNative);
    vm.addNativeTypeFn<VarCurl>(loc, "setOptNative", feralCurlEasySetOptNative);
    vm.addNativeTypeFn<VarCurl>(loc, "perform", feralCurlEasyPerform);
    vm.addNativeTypeFn<VarCurl>(loc, "setProgressCBTickNative", feralCurlSetProgressCBTick);

    setEnumVars(vm, mod, loc);

    return true;
}

DEINIT_MODULE(Curl) { curl_global_cleanup(); }

void setEnumVars(VirtualMachine &vm, VarModule *mod, ModuleLoc loc)
{
    // All the enum values

    // CURLcode
    mod->addNativeVar(vm, "E_OK", "", vm.makeVar<VarInt>(loc, CURLE_OK));
    mod->addNativeVar(vm, "E_UNSUPPORTED_PROTOCOL", "",
                      vm.makeVar<VarInt>(loc, CURLE_UNSUPPORTED_PROTOCOL));
    mod->addNativeVar(vm, "E_FAILED_INIT", "", vm.makeVar<VarInt>(loc, CURLE_FAILED_INIT));
    mod->addNativeVar(vm, "E_URL_MALFORMAT", "", vm.makeVar<VarInt>(loc, CURLE_URL_MALFORMAT));
    mod->addNativeVar(vm, "E_NOT_BUILT_IN", "", vm.makeVar<VarInt>(loc, CURLE_NOT_BUILT_IN));
    mod->addNativeVar(vm, "E_COULDNT_RESOLVE_PROXY", "",
                      vm.makeVar<VarInt>(loc, CURLE_COULDNT_RESOLVE_PROXY));
    mod->addNativeVar(vm, "E_COULDNT_RESOLVE_HOST", "",
                      vm.makeVar<VarInt>(loc, CURLE_COULDNT_RESOLVE_HOST));
    mod->addNativeVar(vm, "E_COULDNT_CONNECT", "", vm.makeVar<VarInt>(loc, CURLE_COULDNT_CONNECT));
#if CURL_AT_LEAST_VERSION(7, 51, 0)
    mod->addNativeVar(vm, "E_WEIRD_SERVER_REPLY", "",
                      vm.makeVar<VarInt>(loc, CURLE_WEIRD_SERVER_REPLY));
#else
    mod->addNativeVar(vm, "E_FTP_WEIRD_SERVER_REPLY", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_SERVER_REPLY));
#endif
    mod->addNativeVar(vm, "E_REMOTE_ACCESS_DENIED", "",
                      vm.makeVar<VarInt>(loc, CURLE_REMOTE_ACCESS_DENIED));
    mod->addNativeVar(vm, "E_FTP_ACCEPT_FAILED", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_ACCEPT_FAILED));
    mod->addNativeVar(vm, "E_FTP_WEIRD_PASS_REPLY", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_PASS_REPLY));
    mod->addNativeVar(vm, "E_FTP_ACCEPT_TIMEOUT", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_ACCEPT_TIMEOUT));
    mod->addNativeVar(vm, "E_FTP_WEIRD_PASV_REPLY", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_PASV_REPLY));
    mod->addNativeVar(vm, "E_FTP_WEIRD_227_FORMAT", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_WEIRD_227_FORMAT));
    mod->addNativeVar(vm, "E_FTP_CANT_GET_HOST", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_CANT_GET_HOST));
    mod->addNativeVar(vm, "E_HTTP2", "", vm.makeVar<VarInt>(loc, CURLE_HTTP2));
    mod->addNativeVar(vm, "E_FTP_COULDNT_SET_TYPE", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_COULDNT_SET_TYPE));
    mod->addNativeVar(vm, "E_PARTIAL_FILE", "", vm.makeVar<VarInt>(loc, CURLE_PARTIAL_FILE));
    mod->addNativeVar(vm, "E_FTP_COULDNT_RETR_FILE", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_COULDNT_RETR_FILE));
    mod->addNativeVar(vm, "E_OBSOLETE20", "", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE20));
    mod->addNativeVar(vm, "E_QUOTE_ERROR", "", vm.makeVar<VarInt>(loc, CURLE_QUOTE_ERROR));
    mod->addNativeVar(vm, "E_HTTP_RETURNED_ERROR", "",
                      vm.makeVar<VarInt>(loc, CURLE_HTTP_RETURNED_ERROR));
    mod->addNativeVar(vm, "E_WRITE_ERROR", "", vm.makeVar<VarInt>(loc, CURLE_WRITE_ERROR));
    mod->addNativeVar(vm, "E_OBSOLETE24", "", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE24));
    mod->addNativeVar(vm, "E_UPLOAD_FAILED", "", vm.makeVar<VarInt>(loc, CURLE_UPLOAD_FAILED));
    mod->addNativeVar(vm, "E_READ_ERROR", "", vm.makeVar<VarInt>(loc, CURLE_READ_ERROR));
    mod->addNativeVar(vm, "E_OUT_OF_MEMORY", "", vm.makeVar<VarInt>(loc, CURLE_OUT_OF_MEMORY));
    mod->addNativeVar(vm, "E_OPERATION_TIMEDOUT", "",
                      vm.makeVar<VarInt>(loc, CURLE_OPERATION_TIMEDOUT));
    mod->addNativeVar(vm, "E_OBSOLETE29", "", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE29));
    mod->addNativeVar(vm, "E_FTP_PORT_FAILED", "", vm.makeVar<VarInt>(loc, CURLE_FTP_PORT_FAILED));
    mod->addNativeVar(vm, "E_FTP_COULDNT_USE_REST", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_COULDNT_USE_REST));
    mod->addNativeVar(vm, "E_OBSOLETE32", "", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE32));
    mod->addNativeVar(vm, "E_RANGE_ERROR", "", vm.makeVar<VarInt>(loc, CURLE_RANGE_ERROR));
    mod->addNativeVar(vm, "E_HTTP_POST_ERROR", "", vm.makeVar<VarInt>(loc, CURLE_HTTP_POST_ERROR));
    mod->addNativeVar(vm, "E_SSL_CONNECT_ERROR", "",
                      vm.makeVar<VarInt>(loc, CURLE_SSL_CONNECT_ERROR));
    mod->addNativeVar(vm, "E_BAD_DOWNLOAD_RESUME", "",
                      vm.makeVar<VarInt>(loc, CURLE_BAD_DOWNLOAD_RESUME));
    mod->addNativeVar(vm, "E_FILE_COULDNT_READ_FILE", "",
                      vm.makeVar<VarInt>(loc, CURLE_FILE_COULDNT_READ_FILE));
    mod->addNativeVar(vm, "E_LDAP_CANNOT_BIND", "",
                      vm.makeVar<VarInt>(loc, CURLE_LDAP_CANNOT_BIND));
    mod->addNativeVar(vm, "E_LDAP_SEARCH_FAILED", "",
                      vm.makeVar<VarInt>(loc, CURLE_LDAP_SEARCH_FAILED));
    mod->addNativeVar(vm, "E_OBSOLETE40", "", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE40));
    mod->addNativeVar(vm, "E_FUNCTION_NOT_FOUND", "",
                      vm.makeVar<VarInt>(loc, CURLE_FUNCTION_NOT_FOUND));
    mod->addNativeVar(vm, "E_ABORTED_BY_CALLBACK", "",
                      vm.makeVar<VarInt>(loc, CURLE_ABORTED_BY_CALLBACK));
    mod->addNativeVar(vm, "E_BAD_FUNCTION_ARGUMENT", "",
                      vm.makeVar<VarInt>(loc, CURLE_BAD_FUNCTION_ARGUMENT));
    mod->addNativeVar(vm, "E_OBSOLETE44", "", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE44));
    mod->addNativeVar(vm, "E_INTERFACE_FAILED", "",
                      vm.makeVar<VarInt>(loc, CURLE_INTERFACE_FAILED));
    mod->addNativeVar(vm, "E_OBSOLETE46", "", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE46));
    mod->addNativeVar(vm, "E_TOO_MANY_REDIRECTS", "",
                      vm.makeVar<VarInt>(loc, CURLE_TOO_MANY_REDIRECTS));
    mod->addNativeVar(vm, "E_UNKNOWN_OPTION", "", vm.makeVar<VarInt>(loc, CURLE_UNKNOWN_OPTION));
    mod->addNativeVar(vm, "E_TELNET_OPTION_SYNTAX", "",
                      vm.makeVar<VarInt>(loc, CURLE_TELNET_OPTION_SYNTAX));
    mod->addNativeVar(vm, "E_OBSOLETE50", "", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE50));
#if CURL_AT_LEAST_VERSION(7, 62, 0)
    mod->addNativeVar(vm, "E_OBSOLETE51", "", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE51));
#endif
    mod->addNativeVar(vm, "E_GOT_NOTHING", "", vm.makeVar<VarInt>(loc, CURLE_GOT_NOTHING));
    mod->addNativeVar(vm, "E_SSL_ENGINE_NOTFOUND", "",
                      vm.makeVar<VarInt>(loc, CURLE_SSL_ENGINE_NOTFOUND));
    mod->addNativeVar(vm, "E_SSL_ENGINE_SETFAILED", "",
                      vm.makeVar<VarInt>(loc, CURLE_SSL_ENGINE_SETFAILED));
    mod->addNativeVar(vm, "E_SEND_ERROR", "", vm.makeVar<VarInt>(loc, CURLE_SEND_ERROR));
    mod->addNativeVar(vm, "E_RECV_ERROR", "", vm.makeVar<VarInt>(loc, CURLE_RECV_ERROR));
    mod->addNativeVar(vm, "E_OBSOLETE57", "", vm.makeVar<VarInt>(loc, CURLE_OBSOLETE57));
    mod->addNativeVar(vm, "E_SSL_CERTPROBLEM", "", vm.makeVar<VarInt>(loc, CURLE_SSL_CERTPROBLEM));
    mod->addNativeVar(vm, "E_SSL_CIPHER", "", vm.makeVar<VarInt>(loc, CURLE_SSL_CIPHER));
    mod->addNativeVar(vm, "E_PEER_FAILED_VERIFICATION", "",
                      vm.makeVar<VarInt>(loc, CURLE_PEER_FAILED_VERIFICATION));
    mod->addNativeVar(vm, "E_BAD_CONTENT_ENCODING", "",
                      vm.makeVar<VarInt>(loc, CURLE_BAD_CONTENT_ENCODING));
    mod->addNativeVar(vm, "E_LDAP_INVALID_URL", "",
                      vm.makeVar<VarInt>(loc, CURLE_LDAP_INVALID_URL));
    mod->addNativeVar(vm, "E_FILESIZE_EXCEEDED", "",
                      vm.makeVar<VarInt>(loc, CURLE_FILESIZE_EXCEEDED));
    mod->addNativeVar(vm, "E_USE_SSL_FAILED", "", vm.makeVar<VarInt>(loc, CURLE_USE_SSL_FAILED));
    mod->addNativeVar(vm, "E_SEND_FAIL_REWIND", "",
                      vm.makeVar<VarInt>(loc, CURLE_SEND_FAIL_REWIND));
    mod->addNativeVar(vm, "E_SSL_ENGINE_INITFAILED", "",
                      vm.makeVar<VarInt>(loc, CURLE_SSL_ENGINE_INITFAILED));
    mod->addNativeVar(vm, "E_LOGIN_DENIED", "", vm.makeVar<VarInt>(loc, CURLE_LOGIN_DENIED));
    mod->addNativeVar(vm, "E_TFTP_NOTFOUND", "", vm.makeVar<VarInt>(loc, CURLE_TFTP_NOTFOUND));
    mod->addNativeVar(vm, "E_TFTP_PERM", "", vm.makeVar<VarInt>(loc, CURLE_TFTP_PERM));
    mod->addNativeVar(vm, "E_REMOTE_DISK_FULL", "",
                      vm.makeVar<VarInt>(loc, CURLE_REMOTE_DISK_FULL));
    mod->addNativeVar(vm, "E_TFTP_ILLEGAL", "", vm.makeVar<VarInt>(loc, CURLE_TFTP_ILLEGAL));
    mod->addNativeVar(vm, "E_TFTP_UNKNOWNID", "", vm.makeVar<VarInt>(loc, CURLE_TFTP_UNKNOWNID));
    mod->addNativeVar(vm, "E_REMOTE_FILE_EXISTS", "",
                      vm.makeVar<VarInt>(loc, CURLE_REMOTE_FILE_EXISTS));
    mod->addNativeVar(vm, "E_TFTP_NOSUCHUSER", "", vm.makeVar<VarInt>(loc, CURLE_TFTP_NOSUCHUSER));
    mod->addNativeVar(vm, "E_CONV_FAILED", "", vm.makeVar<VarInt>(loc, CURLE_CONV_FAILED));
    mod->addNativeVar(vm, "E_CONV_REQD", "", vm.makeVar<VarInt>(loc, CURLE_CONV_REQD));
    mod->addNativeVar(vm, "E_SSL_CACERT_BADFILE", "",
                      vm.makeVar<VarInt>(loc, CURLE_SSL_CACERT_BADFILE));
    mod->addNativeVar(vm, "E_REMOTE_FILE_NOT_FOUND", "",
                      vm.makeVar<VarInt>(loc, CURLE_REMOTE_FILE_NOT_FOUND));
    mod->addNativeVar(vm, "E_SSH", "", vm.makeVar<VarInt>(loc, CURLE_SSH));
    mod->addNativeVar(vm, "E_SSL_SHUTDOWN_FAILED", "",
                      vm.makeVar<VarInt>(loc, CURLE_SSL_SHUTDOWN_FAILED));
    mod->addNativeVar(vm, "E_AGAIN", "", vm.makeVar<VarInt>(loc, CURLE_AGAIN));
    mod->addNativeVar(vm, "E_SSL_CRL_BADFILE", "", vm.makeVar<VarInt>(loc, CURLE_SSL_CRL_BADFILE));
    mod->addNativeVar(vm, "E_SSL_ISSUER_ERROR", "",
                      vm.makeVar<VarInt>(loc, CURLE_SSL_ISSUER_ERROR));
    mod->addNativeVar(vm, "E_FTP_PRET_FAILED", "", vm.makeVar<VarInt>(loc, CURLE_FTP_PRET_FAILED));
    mod->addNativeVar(vm, "E_RTSP_CSEQ_ERROR", "", vm.makeVar<VarInt>(loc, CURLE_RTSP_CSEQ_ERROR));
    mod->addNativeVar(vm, "E_RTSP_SESSION_ERROR", "",
                      vm.makeVar<VarInt>(loc, CURLE_RTSP_SESSION_ERROR));
    mod->addNativeVar(vm, "E_FTP_BAD_FILE_LIST", "",
                      vm.makeVar<VarInt>(loc, CURLE_FTP_BAD_FILE_LIST));
    mod->addNativeVar(vm, "E_CHUNK_FAILED", "", vm.makeVar<VarInt>(loc, CURLE_CHUNK_FAILED));
    mod->addNativeVar(vm, "E_NO_CONNECTION_AVAILABLE", "",
                      vm.makeVar<VarInt>(loc, CURLE_NO_CONNECTION_AVAILABLE));
    mod->addNativeVar(vm, "E_SSL_PINNEDPUBKEYNOTMATCH", "",
                      vm.makeVar<VarInt>(loc, CURLE_SSL_PINNEDPUBKEYNOTMATCH));
    mod->addNativeVar(vm, "E_SSL_INVALIDCERTSTATUS", "",
                      vm.makeVar<VarInt>(loc, CURLE_SSL_INVALIDCERTSTATUS));
#if CURL_AT_LEAST_VERSION(7, 50, 2)
    mod->addNativeVar(vm, "E_HTTP2_STREAM", "", vm.makeVar<VarInt>(loc, CURLE_HTTP2_STREAM));
#endif
#if CURL_AT_LEAST_VERSION(7, 59, 0)
    mod->addNativeVar(vm, "E_RECURSIVE_API_CALL", "",
                      vm.makeVar<VarInt>(loc, CURLE_RECURSIVE_API_CALL));
#endif
#if CURL_AT_LEAST_VERSION(7, 66, 0)
    mod->addNativeVar(vm, "E_AUTH_ERROR", "", vm.makeVar<VarInt>(loc, CURLE_AUTH_ERROR));
#endif
#if CURL_AT_LEAST_VERSION(7, 68, 0)
    mod->addNativeVar(vm, "E_HTTP3", "", vm.makeVar<VarInt>(loc, CURLE_HTTP3));
#endif
#if CURL_AT_LEAST_VERSION(7, 69, 0)
    mod->addNativeVar(vm, "E_QUIC_CONNECT_ERROR", "",
                      vm.makeVar<VarInt>(loc, CURLE_QUIC_CONNECT_ERROR));
#endif

    // CURLMcode
    mod->addNativeVar(vm, "M_CALL_MULTI_PERFORM", "",
                      vm.makeVar<VarInt>(loc, CURLM_CALL_MULTI_PERFORM));
    mod->addNativeVar(vm, "M_OK", "", vm.makeVar<VarInt>(loc, CURLM_OK));
    mod->addNativeVar(vm, "M_BAD_HANDLE", "", vm.makeVar<VarInt>(loc, CURLM_BAD_HANDLE));
    mod->addNativeVar(vm, "M_BAD_EASY_HANDLE", "", vm.makeVar<VarInt>(loc, CURLM_BAD_EASY_HANDLE));
    mod->addNativeVar(vm, "M_OUT_OF_MEMORY", "", vm.makeVar<VarInt>(loc, CURLM_OUT_OF_MEMORY));
    mod->addNativeVar(vm, "M_INTERNAL_ERROR", "", vm.makeVar<VarInt>(loc, CURLM_INTERNAL_ERROR));
    mod->addNativeVar(vm, "M_BAD_SOCKET", "", vm.makeVar<VarInt>(loc, CURLM_BAD_SOCKET));
    mod->addNativeVar(vm, "M_UNKNOWN_OPTION", "", vm.makeVar<VarInt>(loc, CURLM_UNKNOWN_OPTION));
    mod->addNativeVar(vm, "M_ADDED_ALREADY", "", vm.makeVar<VarInt>(loc, CURLM_ADDED_ALREADY));
#if CURL_AT_LEAST_VERSION(7, 59, 0)
    mod->addNativeVar(vm, "M_RECURSIVE_API_CALL", "",
                      vm.makeVar<VarInt>(loc, CURLM_RECURSIVE_API_CALL));
#endif
#if CURL_AT_LEAST_VERSION(7, 68, 0)
    mod->addNativeVar(vm, "WAKEUP_FAILURE", "", vm.makeVar<VarInt>(loc, CURLM_WAKEUP_FAILURE));
#endif
#if CURL_AT_LEAST_VERSION(7, 69, 0)
    mod->addNativeVar(vm, "BAD_FUNCTION_ARGUMENT", "",
                      vm.makeVar<VarInt>(loc, CURLM_BAD_FUNCTION_ARGUMENT));
#endif

    // CURLSHcode
    mod->addNativeVar(vm, "SHE_OK", "", vm.makeVar<VarInt>(loc, CURLSHE_OK));
    mod->addNativeVar(vm, "SHE_BAD_OPTION", "", vm.makeVar<VarInt>(loc, CURLSHE_BAD_OPTION));
    mod->addNativeVar(vm, "SHE_IN_USE", "", vm.makeVar<VarInt>(loc, CURLSHE_IN_USE));
    mod->addNativeVar(vm, "SHE_INVALID", "", vm.makeVar<VarInt>(loc, CURLSHE_INVALID));
    mod->addNativeVar(vm, "SHE_NOMEM", "", vm.makeVar<VarInt>(loc, CURLSHE_NOMEM));
    mod->addNativeVar(vm, "SHE_NOT_BUILT_IN", "", vm.makeVar<VarInt>(loc, CURLSHE_NOT_BUILT_IN));

#if CURL_AT_LEAST_VERSION(7, 62, 0)
    // CURLUcode
    mod->addNativeVar(vm, "UE_OK", "", vm.makeVar<VarInt>(loc, CURLUE_OK));
    mod->addNativeVar(vm, "UE_BAD_HANDLE", "", vm.makeVar<VarInt>(loc, CURLUE_BAD_HANDLE));
    mod->addNativeVar(vm, "UE_BAD_PARTPOINTER", "",
                      vm.makeVar<VarInt>(loc, CURLUE_BAD_PARTPOINTER));
    mod->addNativeVar(vm, "UE_MALFORMED_INPUT", "",
                      vm.makeVar<VarInt>(loc, CURLUE_MALFORMED_INPUT));
    mod->addNativeVar(vm, "UE_BAD_PORT_NUMBER", "",
                      vm.makeVar<VarInt>(loc, CURLUE_BAD_PORT_NUMBER));
    mod->addNativeVar(vm, "UE_UNSUPPORTED_SCHEME", "",
                      vm.makeVar<VarInt>(loc, CURLUE_UNSUPPORTED_SCHEME));
    mod->addNativeVar(vm, "UE_URLDECODE", "", vm.makeVar<VarInt>(loc, CURLUE_URLDECODE));
    mod->addNativeVar(vm, "UE_OUT_OF_MEMORY", "", vm.makeVar<VarInt>(loc, CURLUE_OUT_OF_MEMORY));
    mod->addNativeVar(vm, "UE_USER_NOT_ALLOWED", "",
                      vm.makeVar<VarInt>(loc, CURLUE_USER_NOT_ALLOWED));
    mod->addNativeVar(vm, "UE_UNKNOWN_PART", "", vm.makeVar<VarInt>(loc, CURLUE_UNKNOWN_PART));
    mod->addNativeVar(vm, "UE_NO_SCHEME", "", vm.makeVar<VarInt>(loc, CURLUE_NO_SCHEME));
    mod->addNativeVar(vm, "UE_NO_USER", "", vm.makeVar<VarInt>(loc, CURLUE_NO_USER));
    mod->addNativeVar(vm, "UE_NO_PASSWORD", "", vm.makeVar<VarInt>(loc, CURLUE_NO_PASSWORD));
    mod->addNativeVar(vm, "UE_NO_OPTIONS", "", vm.makeVar<VarInt>(loc, CURLUE_NO_OPTIONS));
    mod->addNativeVar(vm, "UE_NO_HOST", "", vm.makeVar<VarInt>(loc, CURLUE_NO_HOST));
    mod->addNativeVar(vm, "UE_NO_PORT", "", vm.makeVar<VarInt>(loc, CURLUE_NO_PORT));
    mod->addNativeVar(vm, "UE_NO_QUERY", "", vm.makeVar<VarInt>(loc, CURLUE_NO_QUERY));
    mod->addNativeVar(vm, "UE_NO_FRAGMENT", "", vm.makeVar<VarInt>(loc, CURLUE_NO_FRAGMENT));
#endif

    // EASY_OPTS

    // BEHAVIOR OPTIONS
    mod->addNativeVar(vm, "OPT_VERBOSE", "", vm.makeVar<VarInt>(loc, CURLOPT_VERBOSE));
    mod->addNativeVar(vm, "OPT_HEADER", "", vm.makeVar<VarInt>(loc, CURLOPT_HEADER));
    mod->addNativeVar(vm, "OPT_NOPROGRESS", "", vm.makeVar<VarInt>(loc, CURLOPT_NOPROGRESS));
    mod->addNativeVar(vm, "OPT_NOSIGNAL", "", vm.makeVar<VarInt>(loc, CURLOPT_NOSIGNAL));
    mod->addNativeVar(vm, "OPT_WILDCARDMATCH", "", vm.makeVar<VarInt>(loc, CURLOPT_WILDCARDMATCH));

    // CALLBACK OPTIONS
    mod->addNativeVar(vm, "OPT_WRITEFUNCTION", "", vm.makeVar<VarInt>(loc, CURLOPT_WRITEFUNCTION));
    mod->addNativeVar(vm, "OPT_WRITEDATA", "", vm.makeVar<VarInt>(loc, CURLOPT_WRITEDATA));
    mod->addNativeVar(vm, "OPT_READFUNCTION", "", vm.makeVar<VarInt>(loc, CURLOPT_READFUNCTION));
    mod->addNativeVar(vm, "OPT_READDATA", "", vm.makeVar<VarInt>(loc, CURLOPT_READDATA));
    mod->addNativeVar(vm, "OPT_SEEKFUNCTION", "", vm.makeVar<VarInt>(loc, CURLOPT_SEEKFUNCTION));
    mod->addNativeVar(vm, "OPT_SEEKDATA", "", vm.makeVar<VarInt>(loc, CURLOPT_SEEKDATA));
    mod->addNativeVar(vm, "OPT_SOCKOPTFUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SOCKOPTFUNCTION));
    mod->addNativeVar(vm, "OPT_SOCKOPTDATA", "", vm.makeVar<VarInt>(loc, CURLOPT_SOCKOPTDATA));
    mod->addNativeVar(vm, "OPT_OPENSOCKETFUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_OPENSOCKETFUNCTION));
    mod->addNativeVar(vm, "OPT_OPENSOCKETDATA", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_OPENSOCKETDATA));
    mod->addNativeVar(vm, "OPT_CLOSESOCKETFUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_CLOSESOCKETFUNCTION));
    mod->addNativeVar(vm, "OPT_CLOSESOCKETDATA", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_CLOSESOCKETDATA));
    mod->addNativeVar(vm, "OPT_PROGRESSDATA", "", vm.makeVar<VarInt>(loc, CURLOPT_PROGRESSDATA));
    mod->addNativeVar(vm, "OPT_XFERINFOFUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_XFERINFOFUNCTION));
    mod->addNativeVar(vm, "OPT_XFERINFODATA", "", vm.makeVar<VarInt>(loc, CURLOPT_XFERINFODATA));
    mod->addNativeVar(vm, "OPT_HEADERFUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_HEADERFUNCTION));
    mod->addNativeVar(vm, "OPT_HEADERDATA", "", vm.makeVar<VarInt>(loc, CURLOPT_HEADERDATA));
    mod->addNativeVar(vm, "OPT_DEBUGFUNCTION", "", vm.makeVar<VarInt>(loc, CURLOPT_DEBUGFUNCTION));
    mod->addNativeVar(vm, "OPT_DEBUGDATA", "", vm.makeVar<VarInt>(loc, CURLOPT_DEBUGDATA));
    mod->addNativeVar(vm, "OPT_SSL_CTX_FUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSL_CTX_FUNCTION));
    mod->addNativeVar(vm, "OPT_SSL_CTX_DATA", "", vm.makeVar<VarInt>(loc, CURLOPT_SSL_CTX_DATA));
    mod->addNativeVar(vm, "OPT_INTERLEAVEFUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_INTERLEAVEFUNCTION));
    mod->addNativeVar(vm, "OPT_INTERLEAVEDATA", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_INTERLEAVEDATA));
    mod->addNativeVar(vm, "OPT_CHUNK_BGN_FUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_CHUNK_BGN_FUNCTION));
    mod->addNativeVar(vm, "OPT_CHUNK_END_FUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_CHUNK_END_FUNCTION));
    mod->addNativeVar(vm, "OPT_CHUNK_DATA", "", vm.makeVar<VarInt>(loc, CURLOPT_CHUNK_DATA));
    mod->addNativeVar(vm, "OPT_FNMATCH_FUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_FNMATCH_FUNCTION));
    mod->addNativeVar(vm, "OPT_FNMATCH_DATA", "", vm.makeVar<VarInt>(loc, CURLOPT_FNMATCH_DATA));
#if CURL_AT_LEAST_VERSION(7, 54, 0)
    mod->addNativeVar(vm, "OPT_SUPPRESS_CONNECT_HEADERS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SUPPRESS_CONNECT_HEADERS));
#endif
#if CURL_AT_LEAST_VERSION(7, 59, 0)
    mod->addNativeVar(vm, "OPT_RESOLVER_START_FUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_RESOLVER_START_FUNCTION));
    mod->addNativeVar(vm, "OPT_RESOLVER_START_DATA", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_RESOLVER_START_DATA));
#endif

    // ERROR OPTIONS
    mod->addNativeVar(vm, "OPT_ERRORBUFFER", "", vm.makeVar<VarInt>(loc, CURLOPT_ERRORBUFFER));
    mod->addNativeVar(vm, "OPT_STDERR", "", vm.makeVar<VarInt>(loc, CURLOPT_STDERR));
    mod->addNativeVar(vm, "OPT_FAILONERROR", "", vm.makeVar<VarInt>(loc, CURLOPT_FAILONERROR));
#if CURL_AT_LEAST_VERSION(7, 51, 0)
    mod->addNativeVar(vm, "OPT_KEEP_SENDING_ON_ERROR", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_KEEP_SENDING_ON_ERROR));
#endif

    // NETWORK OPTIONS
    mod->addNativeVar(vm, "OPT_URL", "", vm.makeVar<VarInt>(loc, CURLOPT_URL));
    mod->addNativeVar(vm, "OPT_PATH_AS_IS", "", vm.makeVar<VarInt>(loc, CURLOPT_PATH_AS_IS));
    mod->addNativeVar(vm, "OPT_PROTOCOLS_STR", "", vm.makeVar<VarInt>(loc, CURLOPT_PROTOCOLS_STR));
    mod->addNativeVar(vm, "OPT_REDIR_PROTOCOLS_STR", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_REDIR_PROTOCOLS_STR));
    mod->addNativeVar(vm, "OPT_DEFAULT_PROTOCOL", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_DEFAULT_PROTOCOL));
    mod->addNativeVar(vm, "OPT_PROXY", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXY));
#if CURL_AT_LEAST_VERSION(7, 52, 0)
    mod->addNativeVar(vm, "OPT_PRE_PROXY", "", vm.makeVar<VarInt>(loc, CURLOPT_PRE_PROXY));
#endif
    mod->addNativeVar(vm, "OPT_PROXYPORT", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXYPORT));
    mod->addNativeVar(vm, "OPT_PROXYTYPE", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXYTYPE));
    mod->addNativeVar(vm, "OPT_NOPROXY", "", vm.makeVar<VarInt>(loc, CURLOPT_NOPROXY));
    mod->addNativeVar(vm, "OPT_HTTPPROXYTUNNEL", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_HTTPPROXYTUNNEL));
#if CURL_AT_LEAST_VERSION(7, 49, 0)
    mod->addNativeVar(vm, "OPT_CONNECT_TO", "", vm.makeVar<VarInt>(loc, CURLOPT_CONNECT_TO));
#endif
#if CURL_AT_LEAST_VERSION(7, 55, 0)
    mod->addNativeVar(vm, "OPT_SOCKS5_AUTH", "", vm.makeVar<VarInt>(loc, CURLOPT_SOCKS5_AUTH));
#endif
    mod->addNativeVar(vm, "OPT_SOCKS5_GSSAPI_NEC", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SOCKS5_GSSAPI_NEC));
    mod->addNativeVar(vm, "OPT_PROXY_SERVICE_NAME", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SERVICE_NAME));
#if CURL_AT_LEAST_VERSION(7, 60, 0)
    mod->addNativeVar(vm, "OPT_HAPROXYPROTOCOL", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_HAPROXYPROTOCOL));
#endif
    mod->addNativeVar(vm, "OPT_SERVICE_NAME", "", vm.makeVar<VarInt>(loc, CURLOPT_SERVICE_NAME));
    mod->addNativeVar(vm, "OPT_INTERFACE", "", vm.makeVar<VarInt>(loc, CURLOPT_INTERFACE));
    mod->addNativeVar(vm, "OPT_LOCALPORT", "", vm.makeVar<VarInt>(loc, CURLOPT_LOCALPORT));
    mod->addNativeVar(vm, "OPT_LOCALPORTRANGE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_LOCALPORTRANGE));
    mod->addNativeVar(vm, "OPT_DNS_CACHE_TIMEOUT", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_DNS_CACHE_TIMEOUT));
#if CURL_AT_LEAST_VERSION(7, 62, 0)
    mod->addNativeVar(vm, "OPT_DOH_URL", "", vm.makeVar<VarInt>(loc, CURLOPT_DOH_URL));
#endif
    mod->addNativeVar(vm, "OPT_BUFFERSIZE", "", vm.makeVar<VarInt>(loc, CURLOPT_BUFFERSIZE));
    mod->addNativeVar(vm, "OPT_PORT", "", vm.makeVar<VarInt>(loc, CURLOPT_PORT));
#if CURL_AT_LEAST_VERSION(7, 49, 0)
    mod->addNativeVar(vm, "OPT_TCP_FASTOPEN", "", vm.makeVar<VarInt>(loc, CURLOPT_TCP_FASTOPEN));
#endif
    mod->addNativeVar(vm, "OPT_TCP_NODELAY", "", vm.makeVar<VarInt>(loc, CURLOPT_TCP_NODELAY));
    mod->addNativeVar(vm, "OPT_ADDRESS_SCOPE", "", vm.makeVar<VarInt>(loc, CURLOPT_ADDRESS_SCOPE));
    mod->addNativeVar(vm, "OPT_TCP_KEEPALIVE", "", vm.makeVar<VarInt>(loc, CURLOPT_TCP_KEEPALIVE));
    mod->addNativeVar(vm, "OPT_TCP_KEEPIDLE", "", vm.makeVar<VarInt>(loc, CURLOPT_TCP_KEEPIDLE));
    mod->addNativeVar(vm, "OPT_TCP_KEEPINTVL", "", vm.makeVar<VarInt>(loc, CURLOPT_TCP_KEEPINTVL));
    mod->addNativeVar(vm, "OPT_UNIX_SOCKET_PATH", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_UNIX_SOCKET_PATH));
#if CURL_AT_LEAST_VERSION(7, 53, 0)
    mod->addNativeVar(vm, "OPT_ABSTRACT_UNIX_SOCKET", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_ABSTRACT_UNIX_SOCKET));
#endif

    // NAMES and PASSWORDS OPTIONS (Authentication)
    mod->addNativeVar(vm, "OPT_NETRC", "", vm.makeVar<VarInt>(loc, CURLOPT_NETRC));
    mod->addNativeVar(vm, "OPT_NETRC_FILE", "", vm.makeVar<VarInt>(loc, CURLOPT_NETRC_FILE));
    mod->addNativeVar(vm, "OPT_USERPWD", "", vm.makeVar<VarInt>(loc, CURLOPT_USERPWD));
    mod->addNativeVar(vm, "OPT_PROXYUSERPWD", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXYUSERPWD));
    mod->addNativeVar(vm, "OPT_USERNAME", "", vm.makeVar<VarInt>(loc, CURLOPT_USERNAME));
    mod->addNativeVar(vm, "OPT_PASSWORD", "", vm.makeVar<VarInt>(loc, CURLOPT_PASSWORD));
    mod->addNativeVar(vm, "OPT_LOGIN_OPTIONS", "", vm.makeVar<VarInt>(loc, CURLOPT_LOGIN_OPTIONS));
    mod->addNativeVar(vm, "OPT_PROXYUSERNAME", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXYUSERNAME));
    mod->addNativeVar(vm, "OPT_PROXYPASSWORD", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXYPASSWORD));
    mod->addNativeVar(vm, "OPT_HTTPAUTH", "", vm.makeVar<VarInt>(loc, CURLOPT_HTTPAUTH));
    mod->addNativeVar(vm, "OPT_TLSAUTH_USERNAME", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_TLSAUTH_USERNAME));
    mod->addNativeVar(vm, "OPT_TLSAUTH_PASSWORD", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_TLSAUTH_PASSWORD));
    mod->addNativeVar(vm, "OPT_TLSAUTH_TYPE", "", vm.makeVar<VarInt>(loc, CURLOPT_TLSAUTH_TYPE));
#if CURL_AT_LEAST_VERSION(7, 52, 0)
    mod->addNativeVar(vm, "OPT_PROXY_TLSAUTH_USERNAME", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLSAUTH_USERNAME));
    mod->addNativeVar(vm, "OPT_PROXY_TLSAUTH_PASSWORD", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLSAUTH_PASSWORD));
    mod->addNativeVar(vm, "OPT_PROXY_TLSAUTH_TYPE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLSAUTH_TYPE));
#endif
    mod->addNativeVar(vm, "OPT_PROXYAUTH", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXYAUTH));
#if CURL_AT_LEAST_VERSION(7, 66, 0)
    mod->addNativeVar(vm, "OPT_SASL_AUTHZID", "", vm.makeVar<VarInt>(loc, CURLOPT_SASL_AUTHZID));
#endif
#if CURL_AT_LEAST_VERSION(7, 61, 0)
    mod->addNativeVar(vm, "OPT_SASL_IR", "", vm.makeVar<VarInt>(loc, CURLOPT_SASL_IR));
    mod->addNativeVar(vm, "OPT_DISALLOW_USERNAME_IN_URL", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_DISALLOW_USERNAME_IN_URL));
#endif
    mod->addNativeVar(vm, "OPT_XOAUTH2_BEARER", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_XOAUTH2_BEARER));

    // HTTP OPTIONS
    mod->addNativeVar(vm, "OPT_AUTOREFERER", "", vm.makeVar<VarInt>(loc, CURLOPT_AUTOREFERER));
    mod->addNativeVar(vm, "OPT_ACCEPT_ENCODING", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_ACCEPT_ENCODING));
    mod->addNativeVar(vm, "OPT_TRANSFER_ENCODING", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_TRANSFER_ENCODING));
    mod->addNativeVar(vm, "OPT_FOLLOWLOCATION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_FOLLOWLOCATION));
    mod->addNativeVar(vm, "OPT_UNRESTRICTED_AUTH", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_UNRESTRICTED_AUTH));
    mod->addNativeVar(vm, "OPT_MAXREDIRS", "", vm.makeVar<VarInt>(loc, CURLOPT_MAXREDIRS));
    mod->addNativeVar(vm, "OPT_POSTREDIR", "", vm.makeVar<VarInt>(loc, CURLOPT_POSTREDIR));
    mod->addNativeVar(vm, "OPT_POST", "", vm.makeVar<VarInt>(loc, CURLOPT_POST));
    mod->addNativeVar(vm, "OPT_POSTFIELDS", "", vm.makeVar<VarInt>(loc, CURLOPT_POSTFIELDS));
    mod->addNativeVar(vm, "OPT_POSTFIELDSIZE", "", vm.makeVar<VarInt>(loc, CURLOPT_POSTFIELDSIZE));
    mod->addNativeVar(vm, "OPT_POSTFIELDSIZE_LARGE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_POSTFIELDSIZE_LARGE));
    mod->addNativeVar(vm, "OPT_COPYPOSTFIELDS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_COPYPOSTFIELDS));
    mod->addNativeVar(vm, "OPT_REFERER", "", vm.makeVar<VarInt>(loc, CURLOPT_REFERER));
    mod->addNativeVar(vm, "OPT_USERAGENT", "", vm.makeVar<VarInt>(loc, CURLOPT_USERAGENT));
    mod->addNativeVar(vm, "OPT_HTTPHEADER", "", vm.makeVar<VarInt>(loc, CURLOPT_HTTPHEADER));
    mod->addNativeVar(vm, "OPT_HEADEROPT", "", vm.makeVar<VarInt>(loc, CURLOPT_HEADEROPT));
    mod->addNativeVar(vm, "OPT_PROXYHEADER", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXYHEADER));
    mod->addNativeVar(vm, "OPT_HTTP200ALIASES", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_HTTP200ALIASES));
    mod->addNativeVar(vm, "OPT_COOKIE", "", vm.makeVar<VarInt>(loc, CURLOPT_COOKIE));
    mod->addNativeVar(vm, "OPT_COOKIEFILE", "", vm.makeVar<VarInt>(loc, CURLOPT_COOKIEFILE));
    mod->addNativeVar(vm, "OPT_COOKIEJAR", "", vm.makeVar<VarInt>(loc, CURLOPT_COOKIEJAR));
    mod->addNativeVar(vm, "OPT_COOKIESESSION", "", vm.makeVar<VarInt>(loc, CURLOPT_COOKIESESSION));
    mod->addNativeVar(vm, "OPT_COOKIELIST", "", vm.makeVar<VarInt>(loc, CURLOPT_COOKIELIST));
#if CURL_AT_LEAST_VERSION(7, 64, 1)
    mod->addNativeVar(vm, "OPT_ALTSVC", "", vm.makeVar<VarInt>(loc, CURLOPT_ALTSVC));
    mod->addNativeVar(vm, "OPT_ALTSVC_CTRL", "", vm.makeVar<VarInt>(loc, CURLOPT_ALTSVC_CTRL));
#endif
    mod->addNativeVar(vm, "OPT_HTTPGET", "", vm.makeVar<VarInt>(loc, CURLOPT_HTTPGET));
#if CURL_AT_LEAST_VERSION(7, 55, 0)
    mod->addNativeVar(vm, "OPT_REQUEST_TARGET", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_REQUEST_TARGET));
#endif
    mod->addNativeVar(vm, "OPT_HTTP_VERSION", "", vm.makeVar<VarInt>(loc, CURLOPT_HTTP_VERSION));
#if CURL_AT_LEAST_VERSION(7, 64, 0)
    mod->addNativeVar(vm, "OPT_HTTP09_ALLOWED", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_HTTP09_ALLOWED));
    mod->addNativeVar(vm, "OPT_TRAILERFUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_TRAILERFUNCTION));
    mod->addNativeVar(vm, "OPT_TRAILERDATA", "", vm.makeVar<VarInt>(loc, CURLOPT_TRAILERDATA));
#endif
    mod->addNativeVar(vm, "OPT_IGNORE_CONTENT_LENGTH", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_IGNORE_CONTENT_LENGTH));
    mod->addNativeVar(vm, "OPT_HTTP_CONTENT_DECODING", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_HTTP_CONTENT_DECODING));
    mod->addNativeVar(vm, "OPT_HTTP_TRANSFER_DECODING", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_HTTP_TRANSFER_DECODING));
    mod->addNativeVar(vm, "OPT_EXPECT_100_TIMEOUT_MS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_EXPECT_100_TIMEOUT_MS));
    mod->addNativeVar(vm, "OPT_PIPEWAIT", "", vm.makeVar<VarInt>(loc, CURLOPT_PIPEWAIT));
    mod->addNativeVar(vm, "OPT_STREAM_DEPENDS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_STREAM_DEPENDS));
    mod->addNativeVar(vm, "OPT_STREAM_DEPENDS_E", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_STREAM_DEPENDS_E));
    mod->addNativeVar(vm, "OPT_STREAM_WEIGHT", "", vm.makeVar<VarInt>(loc, CURLOPT_STREAM_WEIGHT));

    // SMTP OPTIONS
    mod->addNativeVar(vm, "OPT_MAIL_FROM", "", vm.makeVar<VarInt>(loc, CURLOPT_MAIL_FROM));
    mod->addNativeVar(vm, "OPT_MAIL_RCPT", "", vm.makeVar<VarInt>(loc, CURLOPT_MAIL_RCPT));
    mod->addNativeVar(vm, "OPT_MAIL_AUTH", "", vm.makeVar<VarInt>(loc, CURLOPT_MAIL_AUTH));
#if CURL_AT_LEAST_VERSION(7, 69, 0)
    mod->addNativeVar(vm, "OPT_MAIL_RCPT_ALLLOWFAILS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_MAIL_RCPT_ALLLOWFAILS));
#endif

    // TFTP OPTIONS
    mod->addNativeVar(vm, "OPT_TFTP_BLKSIZE", "", vm.makeVar<VarInt>(loc, CURLOPT_TFTP_BLKSIZE));
#if CURL_AT_LEAST_VERSION(7, 48, 0)
    mod->addNativeVar(vm, "OPT_TFTP_NO_OPTIONS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_TFTP_NO_OPTIONS));
#endif

    // FTP OPTIONS
    mod->addNativeVar(vm, "OPT_FTPPORT", "", vm.makeVar<VarInt>(loc, CURLOPT_FTPPORT));
    mod->addNativeVar(vm, "OPT_QUOTE", "", vm.makeVar<VarInt>(loc, CURLOPT_QUOTE));
    mod->addNativeVar(vm, "OPT_POSTQUOTE", "", vm.makeVar<VarInt>(loc, CURLOPT_POSTQUOTE));
    mod->addNativeVar(vm, "OPT_PREQUOTE", "", vm.makeVar<VarInt>(loc, CURLOPT_PREQUOTE));
    mod->addNativeVar(vm, "OPT_APPEND", "", vm.makeVar<VarInt>(loc, CURLOPT_APPEND));
    mod->addNativeVar(vm, "OPT_FTP_USE_EPRT", "", vm.makeVar<VarInt>(loc, CURLOPT_FTP_USE_EPRT));
    mod->addNativeVar(vm, "OPT_FTP_USE_EPSV", "", vm.makeVar<VarInt>(loc, CURLOPT_FTP_USE_EPSV));
    mod->addNativeVar(vm, "OPT_FTP_USE_PRET", "", vm.makeVar<VarInt>(loc, CURLOPT_FTP_USE_PRET));
    mod->addNativeVar(vm, "OPT_FTP_CREATE_MISSING_DIRS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_FTP_CREATE_MISSING_DIRS));
    mod->addNativeVar(vm, "OPT_FTP_RESPONSE_TIMEOUT", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_FTP_RESPONSE_TIMEOUT));
    mod->addNativeVar(vm, "OPT_FTP_ALTERNATIVE_TO_USER", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_FTP_ALTERNATIVE_TO_USER));
    mod->addNativeVar(vm, "OPT_FTP_SKIP_PASV_IP", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_FTP_SKIP_PASV_IP));
    mod->addNativeVar(vm, "OPT_FTPSSLAUTH", "", vm.makeVar<VarInt>(loc, CURLOPT_FTPSSLAUTH));
    mod->addNativeVar(vm, "OPT_FTP_SSL_CCC", "", vm.makeVar<VarInt>(loc, CURLOPT_FTP_SSL_CCC));
    mod->addNativeVar(vm, "OPT_FTP_ACCOUNT", "", vm.makeVar<VarInt>(loc, CURLOPT_FTP_ACCOUNT));
    mod->addNativeVar(vm, "OPT_FTP_FILEMETHOD", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_FTP_FILEMETHOD));

    // RTSP OPTIONS
    mod->addNativeVar(vm, "OPT_RTSP_REQUEST", "", vm.makeVar<VarInt>(loc, CURLOPT_RTSP_REQUEST));
    mod->addNativeVar(vm, "OPT_RTSP_SESSION_ID", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_RTSP_SESSION_ID));
    mod->addNativeVar(vm, "OPT_RTSP_STREAM_URI", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_RTSP_STREAM_URI));
    mod->addNativeVar(vm, "OPT_RTSP_TRANSPORT", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_RTSP_TRANSPORT));
    mod->addNativeVar(vm, "OPT_RTSP_CLIENT_CSEQ", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_RTSP_CLIENT_CSEQ));
    mod->addNativeVar(vm, "OPT_RTSP_SERVER_CSEQ", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_RTSP_SERVER_CSEQ));

    // PROTOCOL OPTIONS
    mod->addNativeVar(vm, "OPT_TRANSFERTEXT", "", vm.makeVar<VarInt>(loc, CURLOPT_TRANSFERTEXT));
    mod->addNativeVar(vm, "OPT_PROXY_TRANSFER_MODE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TRANSFER_MODE));
    mod->addNativeVar(vm, "OPT_CRLF", "", vm.makeVar<VarInt>(loc, CURLOPT_CRLF));
    mod->addNativeVar(vm, "OPT_RANGE", "", vm.makeVar<VarInt>(loc, CURLOPT_RANGE));
    mod->addNativeVar(vm, "OPT_RESUME_FROM", "", vm.makeVar<VarInt>(loc, CURLOPT_RESUME_FROM));
    mod->addNativeVar(vm, "OPT_RESUME_FROM_LARGE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_RESUME_FROM_LARGE));
#if CURL_AT_LEAST_VERSION(7, 63, 0)
    mod->addNativeVar(vm, "OPT_CURLU", "", vm.makeVar<VarInt>(loc, CURLOPT_CURLU));
#endif
    mod->addNativeVar(vm, "OPT_CUSTOMREQUEST", "", vm.makeVar<VarInt>(loc, CURLOPT_CUSTOMREQUEST));
    mod->addNativeVar(vm, "OPT_FILETIME", "", vm.makeVar<VarInt>(loc, CURLOPT_FILETIME));
    mod->addNativeVar(vm, "OPT_DIRLISTONLY", "", vm.makeVar<VarInt>(loc, CURLOPT_DIRLISTONLY));
    mod->addNativeVar(vm, "OPT_NOBODY", "", vm.makeVar<VarInt>(loc, CURLOPT_NOBODY));
    mod->addNativeVar(vm, "OPT_INFILESIZE", "", vm.makeVar<VarInt>(loc, CURLOPT_INFILESIZE));
    mod->addNativeVar(vm, "OPT_INFILESIZE_LARGE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_INFILESIZE_LARGE));
    mod->addNativeVar(vm, "OPT_UPLOAD", "", vm.makeVar<VarInt>(loc, CURLOPT_UPLOAD));
#if CURL_AT_LEAST_VERSION(7, 62, 0)
    mod->addNativeVar(vm, "OPT_UPLOAD_BUFFERSIZE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_UPLOAD_BUFFERSIZE));
#endif
#if CURL_AT_LEAST_VERSION(7, 56, 0)
    mod->addNativeVar(vm, "OPT_MIMEPOST", "", vm.makeVar<VarInt>(loc, CURLOPT_MIMEPOST));
#endif
    mod->addNativeVar(vm, "OPT_MAXFILESIZE", "", vm.makeVar<VarInt>(loc, CURLOPT_MAXFILESIZE));
    mod->addNativeVar(vm, "OPT_MAXFILESIZE_LARGE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_MAXFILESIZE_LARGE));
    mod->addNativeVar(vm, "OPT_TIMECONDITION", "", vm.makeVar<VarInt>(loc, CURLOPT_TIMECONDITION));
    mod->addNativeVar(vm, "OPT_TIMEVALUE", "", vm.makeVar<VarInt>(loc, CURLOPT_TIMEVALUE));
#if CURL_AT_LEAST_VERSION(7, 59, 0)
    mod->addNativeVar(vm, "OPT_TIMEVALUE_LARGE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_TIMEVALUE_LARGE));
#endif

    // CONNECTION OPTIONS
    mod->addNativeVar(vm, "OPT_TIMEOUT", "", vm.makeVar<VarInt>(loc, CURLOPT_TIMEOUT));
    mod->addNativeVar(vm, "OPT_TIMEOUT_MS", "", vm.makeVar<VarInt>(loc, CURLOPT_TIMEOUT_MS));
    mod->addNativeVar(vm, "OPT_LOW_SPEED_LIMIT", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_LOW_SPEED_LIMIT));
    mod->addNativeVar(vm, "OPT_LOW_SPEED_TIME", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_LOW_SPEED_TIME));
    mod->addNativeVar(vm, "OPT_MAX_SEND_SPEED_LARGE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_MAX_SEND_SPEED_LARGE));
    mod->addNativeVar(vm, "OPT_MAX_RECV_SPEED_LARGE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_MAX_RECV_SPEED_LARGE));
    mod->addNativeVar(vm, "OPT_MAXCONNECTS", "", vm.makeVar<VarInt>(loc, CURLOPT_MAXCONNECTS));
    mod->addNativeVar(vm, "OPT_FRESH_CONNECT", "", vm.makeVar<VarInt>(loc, CURLOPT_FRESH_CONNECT));
    mod->addNativeVar(vm, "OPT_FORBID_REUSE", "", vm.makeVar<VarInt>(loc, CURLOPT_FORBID_REUSE));
#if CURL_AT_LEAST_VERSION(7, 65, 0)
    mod->addNativeVar(vm, "OPT_MAXAGE_CONN", "", vm.makeVar<VarInt>(loc, CURLOPT_MAXAGE_CONN));
#endif
    mod->addNativeVar(vm, "OPT_CONNECTTIMEOUT", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_CONNECTTIMEOUT));
    mod->addNativeVar(vm, "OPT_CONNECTTIMEOUT_MS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_CONNECTTIMEOUT_MS));
    mod->addNativeVar(vm, "OPT_IPRESOLVE", "", vm.makeVar<VarInt>(loc, CURLOPT_IPRESOLVE));
    mod->addNativeVar(vm, "OPT_CONNECT_ONLY", "", vm.makeVar<VarInt>(loc, CURLOPT_CONNECT_ONLY));
    mod->addNativeVar(vm, "OPT_USE_SSL", "", vm.makeVar<VarInt>(loc, CURLOPT_USE_SSL));
    mod->addNativeVar(vm, "OPT_RESOLVE", "", vm.makeVar<VarInt>(loc, CURLOPT_RESOLVE));
    mod->addNativeVar(vm, "OPT_DNS_INTERFACE", "", vm.makeVar<VarInt>(loc, CURLOPT_DNS_INTERFACE));
    mod->addNativeVar(vm, "OPT_DNS_LOCAL_IP4", "", vm.makeVar<VarInt>(loc, CURLOPT_DNS_LOCAL_IP4));
    mod->addNativeVar(vm, "OPT_DNS_LOCAL_IP6", "", vm.makeVar<VarInt>(loc, CURLOPT_DNS_LOCAL_IP6));
    mod->addNativeVar(vm, "OPT_DNS_SERVERS", "", vm.makeVar<VarInt>(loc, CURLOPT_DNS_SERVERS));
#if CURL_AT_LEAST_VERSION(7, 60, 0)
    mod->addNativeVar(vm, "OPT_DNS_SHUFFLE_ADDRESSES", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_DNS_SHUFFLE_ADDRESSES));
#endif
    mod->addNativeVar(vm, "OPT_ACCEPTTIMEOUT_MS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_ACCEPTTIMEOUT_MS));
#if CURL_AT_LEAST_VERSION(7, 59, 0)
    mod->addNativeVar(vm, "OPT_HAPPY_EYEBALLS_TIMEOUT_MS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS));
#endif
#if CURL_AT_LEAST_VERSION(7, 62, 0)
    mod->addNativeVar(vm, "OPT_UPKEEP_INTERVAL_MS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_UPKEEP_INTERVAL_MS));
#endif

    // SSL and SECURITY OPTIONS
    mod->addNativeVar(vm, "OPT_SSLCERT", "", vm.makeVar<VarInt>(loc, CURLOPT_SSLCERT));
    mod->addNativeVar(vm, "OPT_SSLCERTTYPE", "", vm.makeVar<VarInt>(loc, CURLOPT_SSLCERTTYPE));
    mod->addNativeVar(vm, "OPT_SSLKEY", "", vm.makeVar<VarInt>(loc, CURLOPT_SSLKEY));
    mod->addNativeVar(vm, "OPT_SSLKEYTYPE", "", vm.makeVar<VarInt>(loc, CURLOPT_SSLKEYTYPE));
    mod->addNativeVar(vm, "OPT_KEYPASSWD", "", vm.makeVar<VarInt>(loc, CURLOPT_KEYPASSWD));
    mod->addNativeVar(vm, "OPT_SSL_ENABLE_ALPN", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSL_ENABLE_ALPN));
    mod->addNativeVar(vm, "OPT_SSLENGINE", "", vm.makeVar<VarInt>(loc, CURLOPT_SSLENGINE));
    mod->addNativeVar(vm, "OPT_SSLENGINE_DEFAULT", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSLENGINE_DEFAULT));
    mod->addNativeVar(vm, "OPT_SSLVERSION", "", vm.makeVar<VarInt>(loc, CURLOPT_SSLVERSION));
    mod->addNativeVar(vm, "OPT_SSL_VERIFYPEER", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSL_VERIFYPEER));
    mod->addNativeVar(vm, "OPT_SSL_VERIFYHOST", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSL_VERIFYHOST));
    mod->addNativeVar(vm, "OPT_SSL_VERIFYSTATUS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSL_VERIFYSTATUS));
#if CURL_AT_LEAST_VERSION(7, 52, 0)
    mod->addNativeVar(vm, "OPT_PROXY_CAINFO", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_CAINFO));
    mod->addNativeVar(vm, "OPT_PROXY_CAPATH", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_CAPATH));
    mod->addNativeVar(vm, "OPT_PROXY_CRLFILE", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_CRLFILE));
    mod->addNativeVar(vm, "OPT_PROXY_KEYPASSWD", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_KEYPASSWD));
    mod->addNativeVar(vm, "OPT_PROXY_PINNEDPUBLICKEY", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_PINNEDPUBLICKEY));
    mod->addNativeVar(vm, "OPT_PROXY_SSLCERT", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLCERT));
    mod->addNativeVar(vm, "OPT_PROXY_SSLCERTTYPE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLCERTTYPE));
    mod->addNativeVar(vm, "OPT_PROXY_SSLKEY", "", vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLKEY));
    mod->addNativeVar(vm, "OPT_PROXY_SSLKEYTYPE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLKEYTYPE));
    mod->addNativeVar(vm, "OPT_PROXY_SSLVERSION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSLVERSION));
    mod->addNativeVar(vm, "OPT_PROXY_SSL_CIPHER_LIST", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_CIPHER_LIST));
    mod->addNativeVar(vm, "OPT_PROXY_SSL_OPTIONS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_OPTIONS));
    mod->addNativeVar(vm, "OPT_PROXY_SSL_VERIFYHOST", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_VERIFYHOST));
    mod->addNativeVar(vm, "OPT_PROXY_SSL_VERIFYPEER", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_SSL_VERIFYPEER));
#endif
    mod->addNativeVar(vm, "OPT_CAINFO", "", vm.makeVar<VarInt>(loc, CURLOPT_CAINFO));
    mod->addNativeVar(vm, "OPT_ISSUERCERT", "", vm.makeVar<VarInt>(loc, CURLOPT_ISSUERCERT));
    mod->addNativeVar(vm, "OPT_CAPATH", "", vm.makeVar<VarInt>(loc, CURLOPT_CAPATH));
    mod->addNativeVar(vm, "OPT_CRLFILE", "", vm.makeVar<VarInt>(loc, CURLOPT_CRLFILE));
    mod->addNativeVar(vm, "OPT_CERTINFO", "", vm.makeVar<VarInt>(loc, CURLOPT_CERTINFO));
    mod->addNativeVar(vm, "OPT_PINNEDPUBLICKEY", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PINNEDPUBLICKEY));
    mod->addNativeVar(vm, "OPT_SSL_CIPHER_LIST", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSL_CIPHER_LIST));
#if CURL_AT_LEAST_VERSION(7, 61, 0)
    mod->addNativeVar(vm, "OPT_TLS13_CIPHERS", "", vm.makeVar<VarInt>(loc, CURLOPT_TLS13_CIPHERS));
    mod->addNativeVar(vm, "OPT_PROXY_TLS13_CIPHERS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_PROXY_TLS13_CIPHERS));
#endif
    mod->addNativeVar(vm, "OPT_SSL_SESSIONID_CACHE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSL_SESSIONID_CACHE));
    mod->addNativeVar(vm, "OPT_SSL_OPTIONS", "", vm.makeVar<VarInt>(loc, CURLOPT_SSL_OPTIONS));
    mod->addNativeVar(vm, "OPT_KRBLEVEL", "", vm.makeVar<VarInt>(loc, CURLOPT_KRBLEVEL));
    mod->addNativeVar(vm, "OPT_GSSAPI_DELEGATION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_GSSAPI_DELEGATION));

    // SSH OPTIONS
    mod->addNativeVar(vm, "OPT_SSH_AUTH_TYPES", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSH_AUTH_TYPES));
#if CURL_AT_LEAST_VERSION(7, 56, 0)
    mod->addNativeVar(vm, "OPT_SSH_COMPRESSION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSH_COMPRESSION));
#endif
    mod->addNativeVar(vm, "OPT_SSH_HOST_PUBLIC_KEY_MD5", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSH_HOST_PUBLIC_KEY_MD5));
    mod->addNativeVar(vm, "OPT_SSH_PUBLIC_KEYFILE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSH_PUBLIC_KEYFILE));
    mod->addNativeVar(vm, "OPT_SSH_PRIVATE_KEYFILE", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSH_PRIVATE_KEYFILE));
    mod->addNativeVar(vm, "OPT_SSH_KNOWNHOSTS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSH_KNOWNHOSTS));
    mod->addNativeVar(vm, "OPT_SSH_KEYFUNCTION", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_SSH_KEYFUNCTION));
    mod->addNativeVar(vm, "OPT_SSH_KEYDATA", "", vm.makeVar<VarInt>(loc, CURLOPT_SSH_KEYDATA));

    // OTHER OPTIONS
    mod->addNativeVar(vm, "OPT_PRIVATE", "", vm.makeVar<VarInt>(loc, CURLOPT_PRIVATE));
    mod->addNativeVar(vm, "OPT_SHARE", "", vm.makeVar<VarInt>(loc, CURLOPT_SHARE));
    mod->addNativeVar(vm, "OPT_NEW_FILE_PERMS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_NEW_FILE_PERMS));
    mod->addNativeVar(vm, "OPT_NEW_DIRECTORY_PERMS", "",
                      vm.makeVar<VarInt>(loc, CURLOPT_NEW_DIRECTORY_PERMS));

    // TELNET OPTIONS
    mod->addNativeVar(vm, "OPT_TELNETOPTIONS", "", vm.makeVar<VarInt>(loc, CURLOPT_TELNETOPTIONS));

    // CURLINFO

    mod->addNativeVar(vm, "INFO_EFFECTIVE_URL", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_EFFECTIVE_URL));
    mod->addNativeVar(vm, "INFO_RESPONSE_CODE", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_RESPONSE_CODE));
    mod->addNativeVar(vm, "INFO_TOTAL_TIME", "", vm.makeVar<VarInt>(loc, CURLINFO_TOTAL_TIME));
    mod->addNativeVar(vm, "INFO_NAMELOOKUP_TIME", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_NAMELOOKUP_TIME));
    mod->addNativeVar(vm, "INFO_CONNECT_TIME", "", vm.makeVar<VarInt>(loc, CURLINFO_CONNECT_TIME));
    mod->addNativeVar(vm, "INFO_PRETRANSFER_TIME", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_PRETRANSFER_TIME));
    mod->addNativeVar(vm, "INFO_SIZE_UPLOAD_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_SIZE_UPLOAD_T));
    mod->addNativeVar(vm, "INFO_SIZE_DOWNLOAD_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_SIZE_DOWNLOAD_T));
    mod->addNativeVar(vm, "INFO_SPEED_DOWNLOAD_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_SPEED_DOWNLOAD_T));
    mod->addNativeVar(vm, "INFO_SPEED_UPLOAD_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_SPEED_UPLOAD_T));
    mod->addNativeVar(vm, "INFO_HEADER_SIZE", "", vm.makeVar<VarInt>(loc, CURLINFO_HEADER_SIZE));
    mod->addNativeVar(vm, "INFO_REQUEST_SIZE", "", vm.makeVar<VarInt>(loc, CURLINFO_REQUEST_SIZE));
    mod->addNativeVar(vm, "INFO_SSL_VERIFYRESULT", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_SSL_VERIFYRESULT));
    mod->addNativeVar(vm, "INFO_FILETIME", "", vm.makeVar<VarInt>(loc, CURLINFO_FILETIME));
    mod->addNativeVar(vm, "INFO_FILETIME_T", "", vm.makeVar<VarInt>(loc, CURLINFO_FILETIME_T));
    mod->addNativeVar(vm, "INFO_CONTENT_LENGTH_DOWNLOAD_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T));
    mod->addNativeVar(vm, "INFO_CONTENT_LENGTH_UPLOAD_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_CONTENT_LENGTH_UPLOAD_T));
    mod->addNativeVar(vm, "INFO_STARTTRANSFER_TIME", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_STARTTRANSFER_TIME));
    mod->addNativeVar(vm, "INFO_CONTENT_TYPE", "", vm.makeVar<VarInt>(loc, CURLINFO_CONTENT_TYPE));
    mod->addNativeVar(vm, "INFO_REDIRECT_TIME", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_REDIRECT_TIME));
    mod->addNativeVar(vm, "INFO_REDIRECT_COUNT", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_REDIRECT_COUNT));
    mod->addNativeVar(vm, "INFO_PRIVATE", "", vm.makeVar<VarInt>(loc, CURLINFO_PRIVATE));
    mod->addNativeVar(vm, "INFO_HTTP_CONNECTCODE", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_HTTP_CONNECTCODE));
    mod->addNativeVar(vm, "INFO_HTTPAUTH_AVAIL", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_HTTPAUTH_AVAIL));
    mod->addNativeVar(vm, "INFO_PROXYAUTH_AVAIL", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_PROXYAUTH_AVAIL));
    mod->addNativeVar(vm, "INFO_OS_ERRNO", "", vm.makeVar<VarInt>(loc, CURLINFO_OS_ERRNO));
    mod->addNativeVar(vm, "INFO_NUM_CONNECTS", "", vm.makeVar<VarInt>(loc, CURLINFO_NUM_CONNECTS));
    mod->addNativeVar(vm, "INFO_SSL_ENGINES", "", vm.makeVar<VarInt>(loc, CURLINFO_SSL_ENGINES));
    mod->addNativeVar(vm, "INFO_COOKIELIST", "", vm.makeVar<VarInt>(loc, CURLINFO_COOKIELIST));
    mod->addNativeVar(vm, "INFO_FTP_ENTRY_PATH", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_FTP_ENTRY_PATH));
    mod->addNativeVar(vm, "INFO_REDIRECT_URL", "", vm.makeVar<VarInt>(loc, CURLINFO_REDIRECT_URL));
    mod->addNativeVar(vm, "INFO_PRIMARY_IP", "", vm.makeVar<VarInt>(loc, CURLINFO_PRIMARY_IP));
    mod->addNativeVar(vm, "INFO_APPCONNECT_TIME", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_APPCONNECT_TIME));
    mod->addNativeVar(vm, "INFO_CERTINFO", "", vm.makeVar<VarInt>(loc, CURLINFO_CERTINFO));
    mod->addNativeVar(vm, "INFO_CONDITION_UNMET", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_CONDITION_UNMET));
    mod->addNativeVar(vm, "INFO_RTSP_SESSION_ID", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_RTSP_SESSION_ID));
    mod->addNativeVar(vm, "INFO_RTSP_CLIENT_CSEQ", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_RTSP_CLIENT_CSEQ));
    mod->addNativeVar(vm, "INFO_RTSP_SERVER_CSEQ", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_RTSP_SERVER_CSEQ));
    mod->addNativeVar(vm, "INFO_RTSP_CSEQ_RECV", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_RTSP_CSEQ_RECV));
    mod->addNativeVar(vm, "INFO_PRIMARY_PORT", "", vm.makeVar<VarInt>(loc, CURLINFO_PRIMARY_PORT));
    mod->addNativeVar(vm, "INFO_LOCAL_IP", "", vm.makeVar<VarInt>(loc, CURLINFO_LOCAL_IP));
    mod->addNativeVar(vm, "INFO_LOCAL_PORT", "", vm.makeVar<VarInt>(loc, CURLINFO_LOCAL_PORT));
    mod->addNativeVar(vm, "INFO_ACTIVESOCKET", "", vm.makeVar<VarInt>(loc, CURLINFO_ACTIVESOCKET));
    mod->addNativeVar(vm, "INFO_TLS_SSL_PTR", "", vm.makeVar<VarInt>(loc, CURLINFO_TLS_SSL_PTR));
    mod->addNativeVar(vm, "INFO_HTTP_VERSION", "", vm.makeVar<VarInt>(loc, CURLINFO_HTTP_VERSION));
    mod->addNativeVar(vm, "INFO_PROXY_SSL_VERIFYRESULT", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_PROXY_SSL_VERIFYRESULT));
    mod->addNativeVar(vm, "INFO_SCHEME", "", vm.makeVar<VarInt>(loc, CURLINFO_SCHEME));
    mod->addNativeVar(vm, "INFO_TOTAL_TIME_T", "", vm.makeVar<VarInt>(loc, CURLINFO_TOTAL_TIME_T));
    mod->addNativeVar(vm, "INFO_NAMELOOKUP_TIME_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_NAMELOOKUP_TIME_T));
    mod->addNativeVar(vm, "INFO_CONNECT_TIME_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_CONNECT_TIME_T));
    mod->addNativeVar(vm, "INFO_PRETRANSFER_TIME_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_PRETRANSFER_TIME_T));
    mod->addNativeVar(vm, "INFO_STARTTRANSFER_TIME_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_STARTTRANSFER_TIME_T));
    mod->addNativeVar(vm, "INFO_REDIRECT_TIME_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_REDIRECT_TIME_T));
    mod->addNativeVar(vm, "INFO_APPCONNECT_TIME_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_APPCONNECT_TIME_T));
    mod->addNativeVar(vm, "INFO_RETRY_AFTER", "", vm.makeVar<VarInt>(loc, CURLINFO_RETRY_AFTER));
    mod->addNativeVar(vm, "INFO_EFFECTIVE_METHOD", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_EFFECTIVE_METHOD));
    mod->addNativeVar(vm, "INFO_PROXY_ERROR", "", vm.makeVar<VarInt>(loc, CURLINFO_PROXY_ERROR));
    mod->addNativeVar(vm, "INFO_REFERER", "", vm.makeVar<VarInt>(loc, CURLINFO_REFERER));
    mod->addNativeVar(vm, "INFO_CAINFO", "", vm.makeVar<VarInt>(loc, CURLINFO_CAINFO));
    mod->addNativeVar(vm, "INFO_CAPATH", "", vm.makeVar<VarInt>(loc, CURLINFO_CAPATH));
    mod->addNativeVar(vm, "INFO_XFER_ID", "", vm.makeVar<VarInt>(loc, CURLINFO_XFER_ID));
    mod->addNativeVar(vm, "INFO_CONN_ID", "", vm.makeVar<VarInt>(loc, CURLINFO_CONN_ID));
    mod->addNativeVar(vm, "INFO_QUEUE_TIME_T", "", vm.makeVar<VarInt>(loc, CURLINFO_QUEUE_TIME_T));
    mod->addNativeVar(vm, "INFO_USED_PROXY", "", vm.makeVar<VarInt>(loc, CURLINFO_USED_PROXY));
    mod->addNativeVar(vm, "INFO_POSTTRANSFER_TIME_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_POSTTRANSFER_TIME_T));
    mod->addNativeVar(vm, "INFO_EARLYDATA_SENT_T", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_EARLYDATA_SENT_T));
    mod->addNativeVar(vm, "INFO_HTTPAUTH_USED", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_HTTPAUTH_USED));
    mod->addNativeVar(vm, "INFO_PROXYAUTH_USED", "",
                      vm.makeVar<VarInt>(loc, CURLINFO_PROXYAUTH_USED));
    mod->addNativeVar(vm, "INFO_LASTONE", "", vm.makeVar<VarInt>(loc, CURLINFO_LASTONE));
}

} // namespace fer