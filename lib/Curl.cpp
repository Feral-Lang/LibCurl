#include "Curl.hpp"

namespace fer
{

constexpr size_t CURL_DEFAULT_PROGRESS_INTERVAL_TICK_MAX = 10;

void setEnumVars(VirtualMachine &vm, ModuleLoc loc);

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Callbacks ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

int curlEasyProgressCallback(void *ptr, curl_off_t dlTotal, curl_off_t dlDone, curl_off_t ulTotal,
                             curl_off_t ulDone)
{
    // ensure that the file to be downloaded is not empty
    // because that would cause a division by zero error later on
    if(dlTotal <= 0 && ulTotal <= 0) return 0;

    VarCurlEasy *c = (VarCurlEasy *)ptr;

    if(!c->getCbVM() || !c->getProgressCB()) return 0;

    size_t &intervalTick = c->getProgIntervalTick();
    if(intervalTick < c->getProgIntervalTickMax()) {
        ++intervalTick;
        return 0;
    }
    intervalTick = 0;

    VirtualMachine &vm = *c->getCbVM();
    ModuleLoc loc      = c->getCbLoc();
    VarClosure *progCb = c->getProgressCB();
    VarFlt dt(loc, dlTotal), dd(loc, dlDone), ut(loc, ulTotal), ud(loc, ulDone);
    vm.incVarRef(&dt);
    vm.incVarRef(&dd);
    vm.incVarRef(&ut);
    vm.incVarRef(&ud);
    Array<Var *, 5> args{nullptr, &dt, &dd, &ut, &ud};
    Var *res = nullptr;
    if(!vm.callVarAndExpect<VarNil>(loc, "progCb", progCb, res, args, nullptr)) {
        vm.fail(loc, "failed to call progress callback, check error above");
        return 1;
    }
    vm.decVarRef(res);
    return 0;
}

size_t curlEasyWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    VarCurlEasy *c = (VarCurlEasy *)userdata;
    if(!c->getCbVM() || !c->getWriteCB()) return size * nmemb; // returning zero is an error

    VirtualMachine &vm  = *c->getCbVM();
    ModuleLoc loc       = c->getCbLoc();
    VarClosure *writeCb = c->getWriteCB();
    VarBytebuffer buf(loc, 0, nmemb * size, (unsigned char *)ptr);
    vm.incVarRef(&buf);
    Array<Var *, 2> args{nullptr, &buf};
    Var *res = nullptr;
    if(!vm.callVarAndExpect<VarNil>(loc, "writeCb", writeCb, res, args, nullptr)) {
        vm.fail(loc, "failed to call write callback, check error above");
        return 0;
    }
    vm.decVarRef(res);
    return size * nmemb;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////// VarCurlEasy ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurlEasy::VarCurlEasy(ModuleLoc loc, CURL *val)
    : Var(loc), val(val), cbdata(nullptr, loc), progCB(nullptr), writeCB(nullptr),
      progIntervalTick(0), progIntervalTickMax(CURL_DEFAULT_PROGRESS_INTERVAL_TICK_MAX), done(false)
{}
VarCurlEasy::~VarCurlEasy()
{
    clearMimeData();
    curl_easy_cleanup(val);
}

void VarCurlEasy::onCreate(VirtualMachine &vm) {}
void VarCurlEasy::onDestroy(VirtualMachine &vm)
{
    setProgressCB(vm, nullptr);
    setWriteCB(vm, nullptr);
}

void VarCurlEasy::setCbData(VirtualMachine *vm, ModuleLoc loc)
{
    cbdata.vm  = vm;
    cbdata.loc = loc;
    curl_easy_setopt(val, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(val, CURLOPT_WRITEDATA, this);
}

void VarCurlEasy::setProgressCB(VirtualMachine &vm, VarClosure *_progCB)
{
    if(progCB) vm.decVarRef(progCB);
    progCB = _progCB;
    if(progCB) vm.incVarRef(progCB);
}
void VarCurlEasy::setWriteCB(VirtualMachine &vm, VarClosure *_writeCB)
{
    if(writeCB) vm.decVarRef(writeCB);
    writeCB = _writeCB;
    if(writeCB) vm.incVarRef(writeCB);
}

bool VarCurlEasy::createMime(VirtualMachine &vm, ModuleLoc loc, Var *data, curl_mime *&mime)
{
    if(data->is<VarMap>() && as<VarMap>(data)->getVal().empty()) return true;

    mimelist.push_front(curl_mime_init(val));
    mime = mimelist.front();
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
                mime = nullptr;
                return false;
            }
            const String &str   = as<VarStr>(v)->getVal();
            curl_mimepart *part = curl_mime_addpart(mime);
            curl_mime_name(part, item.first.c_str());
            curl_mime_data(part, str.c_str(), str.size());
            vm.decVarRef(v);
        }
    }
    return true;
}
void VarCurlEasy::clearMimeData()
{
    while(!mimelist.empty()) {
        curl_mime_free(mimelist.front());
        mimelist.pop_front();
    }
}

bool VarCurlEasy::createSList(VirtualMachine &vm, ModuleLoc loc, Var *data, curl_slist *&lst)
{
    if(data->is<VarMap>() && as<VarMap>(data)->getVal().empty()) return true;

    sllist.push_front(nullptr);
    lst = sllist.front();
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
                lst = nullptr;
                return false;
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
    return true;
}
void VarCurlEasy::clearSList()
{
    while(!sllist.empty()) {
        curl_slist_free_all(sllist.front());
        sllist.pop_front();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// VarCurlMulti ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

VarCurlMulti::VarCurlMulti(ModuleLoc loc, CURLM *val, size_t pollMs)
    : Var(loc), val(val), pollMs(pollMs)
{}
VarCurlMulti::~VarCurlMulti() { curl_multi_cleanup(val); }

void VarCurlMulti::onCreate(VirtualMachine &vm)
{ easyHandles = vm.makeVar<VarVec>(getLoc(), 5, true); }
void VarCurlMulti::onDestroy(VirtualMachine &vm) { vm.decVarRef(easyHandles); }

void VarCurlMulti::addEasy(VirtualMachine &vm, VarCurlEasy *easy, bool iref)
{
    LockGuard<RecursiveMutex> _(mtx);
    curl_multi_add_handle(val, easy->getVal());
    easyHandles->push(vm, easy, iref);
}

VarCurlEasy *VarCurlMulti::remEasy(VirtualMachine &vm, ModuleLoc loc, CURL *easy, bool dref)
{
    LockGuard<RecursiveMutex> _(mtx);
    auto &handles = easyHandles->getVal();
    for(size_t i = 0; i < handles.size(); ++i) {
        VarCurlEasy *h = as<VarCurlEasy>(easyHandles->at(i));
        if(h->getVal() != easy) continue;
        curl_multi_remove_handle(val, easy);
        easyHandles->erase(vm, i, dref);
        return h;
    }
    vm.fail(loc, "failed to get easy handle for the CURL C object");
    return nullptr;
}

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

FERAL_FUNC(feralCurlMultiStrErr, 1, false,
           "  fn(errCode) -> Str\n"
           "Returns the string representation of the curl multi error code `errCode`.")
{
    EXPECT(VarInt, args[1], "error code");
    CURLMcode code = (CURLMcode)as<VarInt>(args[1])->getVal();
    return vm.makeVar<VarStr>(loc, curl_multi_strerror(code));
}

FERAL_FUNC(feralCurlEasyStrErr, 1, false,
           "  fn(errCode) -> Str\n"
           "Returns the string representation of the curl easy error code `errCode`.")
{
    EXPECT(VarInt, args[1], "error code");
    CURLcode code = (CURLcode)as<VarInt>(args[1])->getVal();
    return vm.makeVar<VarStr>(loc, curl_easy_strerror(code));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// VarCurlMulti ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

FERAL_FUNC(feralCurlMultiInit, 0, false,
           "  fn(.kw) -> CurlMulti\n"
           "Creates and returns a Curl Multi instance which can be used to create easy handles and "
           "use them to perform network operations.\n"
           "Takes the following optional keyword arguments:\n"
           "- `poll = <int>` - duration in milliseconds to wait when polling. (default: 100)")
{
    CURLM *curl = curl_multi_init();
    if(!curl) {
        vm.fail(loc, "failed to run curl_multi_init()");
        return nullptr;
    }
    size_t pollMs = 100;
    if(Var *pollVar = assnArgs->getAttr("poll")) {
        EXPECT(VarInt, pollVar, "poll duration in ms");
        pollMs = as<VarInt>(pollVar)->getVal();
    }
    return vm.makeVar<VarCurlMulti>(loc, curl, pollMs);
}

FERAL_FUNC(curlMultiAddEasy, 1, true,
           "  var.fn(easy...) -> Nil\n"
           "Adds all `easy` instances to the multi handle `var`.")
{
    VarCurlMulti *m = as<VarCurlMulti>(args[0]);
    for(size_t i = 1; i < args.size(); ++i) {
        EXPECT(VarCurlEasy, args[i], "easy handle");
        VarCurlEasy *c = as<VarCurlEasy>(args[i]);
        m->addEasy(vm, c, true);
    }
    return vm.getNil();
}

FERAL_FUNC(curlMultiPerform, 0, false,
           "  var.fn() -> Int\n"
           "Performs the required operations on the Curl multi object `var`.\n"
           "Returns the status code (CURLMcode) of the operation.")
{
    VarCurlMulti *m = as<VarCurlMulti>(args[0]);
    LockGuard<RecursiveMutex> _(m->getMutex());
    for(auto &h : m->getEasyHandles()->getVal()) as<VarCurlEasy>(h)->setCbData(&vm, loc);

    int running   = 1;
    CURLMcode res = curl_multi_perform(m->getVal(), &running);
    if(running) res = curl_multi_poll(m->getVal(), NULL, 0, m->getPollMs(), NULL);

    int queued = 0;
    while(CURLMsg *msg = curl_multi_info_read(m->getVal(), &queued)) {
        if(msg->msg != CURLMSG_DONE) continue;
        VarCurlEasy *h = m->remEasy(vm, loc, msg->easy_handle, false);
        if(!h) return nullptr;
        h->setDone(true);
        vm.decVarRef(h);
    }
    return vm.makeVar<VarInt>(loc, res);
}

FERAL_FUNC(curlMultiSetOptNative, 2, true,
           "  var.fn(option, suboption/value, args...) -> Int\n"
           "Sets the `option` in CurlMulti `var` as one/more "
           "`suboption/value` and returns the integer result.\n"
           "Here, type of `suboption/value` is dependent on the `option` being used.")
{
    EXPECT(VarInt, args[1], "option type (CURLMOPT_*)");
    VarCurlMulti *m = as<VarCurlMulti>(args[0]);
    CURLM *curl     = m->getVal();
    int opt         = as<VarInt>(args[1])->getVal();
    Var *arg        = args[2];

    CURLMcode res = CURLM_OK;
    // manually handle each of the options and work accordingly
    switch(opt) {
    case CURLMOPT_MAXCONNECTS:
    case CURLMOPT_MAX_CONCURRENT_STREAMS:
    case CURLMOPT_MAX_HOST_CONNECTIONS:
    case CURLMOPT_MAX_TOTAL_CONNECTIONS:
    case CURLMOPT_NETWORK_CHANGED:
    case CURLMOPT_PIPELINING:
    case CURLMOPT_QUICK_EXIT:
    case CURLMOPT_RESOLVE_THREADS_MAX: {
        EXPECT(VarInt, arg, "option value");
        res = curl_multi_setopt(curl, (CURLMoption)opt, as<VarInt>(arg)->getVal());
        break;
    }
    default: {
        vm.fail(loc, "operation is not yet implemented");
        return nullptr;
    }
    }
    return vm.makeVar<VarInt>(loc, res);
}

FERAL_FUNC(curlMultiEmpty, 0, false,
           "  var.fn() -> Bool\n"
           "Returns `true` if there is no easy handle in the multi instance `var`.")
{
    VarCurlMulti *m = as<VarCurlMulti>(args[0]);
    LockGuard<RecursiveMutex> _(m->getMutex());
    return m->getEasyHandles()->empty() ? vm.getTrue() : vm.getFalse();
}

FERAL_FUNC(curlMultiLen, 0, false,
           "  var.fn() -> Bool\n"
           "Returns number of easy handles contained in the multi instance `var`.")
{
    VarCurlMulti *m = as<VarCurlMulti>(args[0]);
    LockGuard<RecursiveMutex> _(m->getMutex());
    return vm.makeVar<VarInt>(loc, m->getEasyHandles()->size());
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////// VarCurlEasy ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

FERAL_FUNC(feralCurlEasyInit, 0, false,
           "  fn() -> CurlEasy\n"
           "Creates and returns a Curl (Easy) instance which can be used to perform network "
           "operations by calling perform() on it or adding it to a multi handle.")
{
    CURL *c = curl_easy_init();
    if(!c) {
        vm.fail(loc, "curl_easy_init() failed");
        return nullptr;
    }
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, curlEasyProgressCallback);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlEasyWriteCallback);
    return vm.makeVar<VarCurlEasy>(loc, c);
}

FERAL_FUNC(curlEasyPerform, 0, false,
           "  var.fn() -> Int\n"
           "Performs the required operations on the Curl object `var` and returns the status code "
           "(CURLcode) of the finished operation.")
{
    VarCurlEasy *c = as<VarCurlEasy>(args[0]);
    c->setCbData(&vm, loc);
    CURLcode res = curl_easy_perform(c->getVal());
    c->setDone(true);
    return vm.makeVar<VarInt>(loc, res);
}

FERAL_FUNC(curlEasyGetHeaderValue, 1, false,
           "  var.fn(name) -> Str | Nil\n"
           "Get the value for the given header `name`.")
{
    EXPECT(VarStr, args[1], "header name");
    VarCurlEasy *curl  = as<VarCurlEasy>(args[0]);
    const String &name = as<VarStr>(args[1])->getVal();
    struct curl_header *header;
    int res = curl_easy_header(curl->getVal(), name.c_str(), 0, CURLH_HEADER, -1, &header);
    if(res != CURLHE_OK) return vm.getNil();
    return vm.makeVar<VarStr>(loc, header->value);
}

FERAL_FUNC(curlEasyGetInfo, 1, false,
           "  var.fn(option) -> Int | Str\n"
           "Gets the info for the Curl `option` (CURLINFO_*) in the curl object `var`.\n"
           "Returns it as an integer/string depending on the option.")
{
    EXPECT(VarInt, args[1], "option type (CURLINFO_*)");
    CURL *curl = as<VarCurlEasy>(args[0])->getVal();
    int opt    = as<VarInt>(args[1])->getVal();

    // manually handle each of the options and work accordingly
    switch(opt) {
    // long / curl_off_t
    case CURLINFO_RESPONSE_CODE:
    case CURLINFO_HTTP_CONNECTCODE:
    case CURLINFO_FILETIME:
    case CURLINFO_REDIRECT_COUNT:
    case CURLINFO_HEADER_SIZE:
    case CURLINFO_REQUEST_SIZE:
    case CURLINFO_SSL_VERIFYRESULT:
    case CURLINFO_PROXY_SSL_VERIFYRESULT:
    case CURLINFO_HTTPAUTH_AVAIL:
    case CURLINFO_PROXYAUTH_AVAIL:
    case CURLINFO_OS_ERRNO:
    case CURLINFO_NUM_CONNECTS:
    case CURLINFO_PRIMARY_PORT:
    case CURLINFO_LOCAL_PORT:
    case CURLINFO_CONDITION_UNMET:
    case CURLINFO_RTSP_CLIENT_CSEQ:
    case CURLINFO_RTSP_SERVER_CSEQ:
    case CURLINFO_RTSP_CSEQ_RECV:
    case CURLINFO_HTTP_VERSION:
    case CURLINFO_FILETIME_T:
    case CURLINFO_SIZE_UPLOAD_T:
    case CURLINFO_SIZE_DOWNLOAD_T:
    case CURLINFO_SPEED_DOWNLOAD_T:
    case CURLINFO_SPEED_UPLOAD_T:
    case CURLINFO_CONTENT_LENGTH_DOWNLOAD_T:
    case CURLINFO_CONTENT_LENGTH_UPLOAD_T:
    case CURLINFO_RETRY_AFTER:
    case CURLINFO_TOTAL_TIME_T:
    case CURLINFO_NAMELOOKUP_TIME_T:
    case CURLINFO_CONNECT_TIME_T:
    case CURLINFO_APPCONNECT_TIME_T:
    case CURLINFO_PRETRANSFER_TIME_T:
    case CURLINFO_STARTTRANSFER_TIME_T:
    case CURLINFO_REDIRECT_TIME_T:
    case CURLINFO_QUEUE_TIME_T: {
        long val;
        curl_easy_getinfo(curl, (CURLINFO)opt, &val);
        return vm.makeVar<VarInt>(loc, val);
    }
    // char * (CURLINFO_STRING)
    case CURLINFO_EFFECTIVE_URL:
    case CURLINFO_CONTENT_TYPE:
    case CURLINFO_REDIRECT_URL:
    case CURLINFO_PRIMARY_IP:
    case CURLINFO_LOCAL_IP:
    case CURLINFO_FTP_ENTRY_PATH:
    case CURLINFO_SCHEME:
    case CURLINFO_RTSP_SESSION_ID:
    case CURLINFO_CAPATH:
    case CURLINFO_CAINFO:
    case CURLINFO_REFERER: {
        char *val;
        curl_easy_getinfo(curl, (CURLINFO)opt, &val);
        return vm.makeVar<VarStr>(loc, val);
    }
    // int (curl_socket_t)
    case CURLINFO_ACTIVESOCKET: {
        int val;
        curl_easy_getinfo(curl, (CURLINFO)opt, &val);
        return vm.makeVar<VarInt>(loc, val);
    }
    default: {
        vm.fail(loc, "operation is not yet implemented");
        return nullptr;
    }
    }
}

FERAL_FUNC(curlEasySetOptNative, 2, true,
           "  var.fn(option, suboption/value, args...) -> Int\n"
           "Sets the `option` in Curl `var` as with one/more `suboption/value` and returns the "
           "integer result.\n"
           "Here, type and count of `suboption/value` are dependent on the `option` being used.")
{
    EXPECT(VarInt, args[1], "option type (CURL_OPT_*)");
    VarCurlEasy *varCurl = as<VarCurlEasy>(args[0]);
    CURL *curl           = varCurl->getVal();
    int opt              = as<VarInt>(args[1])->getVal();
    Var *arg             = args[2];

    int res = CURLE_OK;
    // manually handle each of the options and work accordingly
    switch(opt) {
    case CURLOPT_MIMEPOST: {
        EXPECT(VarMap, arg, "name-data pairs");
        curl_mime *mime = nullptr;
        if(!varCurl->createMime(vm, loc, as<VarMap>(arg), mime)) {
            vm.fail(loc, "failed to create mime from the given map, see error above");
            return nullptr;
        }
        if(mime) res = curl_easy_setopt(curl, (CURLoption)opt, mime);
        break;
    }
    case CURLOPT_XFERINFOFUNCTION: {
        if(arg->is<VarNil>()) {
            varCurl->setProgressCB(vm, nullptr);
            break;
        }
        EXPECT_CALLABLE(arg, "xfer info function");
        if(arg->is<VarFn>()) {
            VarFn *f = as<VarFn>(arg);
            if(f->getParamCount() < 4) {
                vm.fail(loc, "expected function to have at least 4",
                        " parameters for this option, found: ", f->getParamCount());
                return nullptr;
            }
        }
        VarClosure *cl = vm.makeVar<VarClosure>(loc, arg);
        for(size_t i = 3; i < args.size(); ++i) { cl->push(vm, args[i], true); }
        varCurl->setProgressCB(vm, cl);
        break;
    }
    case CURLOPT_WRITEFUNCTION: {
        if(arg->is<VarNil>()) {
            varCurl->setWriteCB(vm, nullptr);
            break;
        }
        EXPECT_CALLABLE(arg, "write function");
        if(arg->is<VarFn>()) {
            VarFn *f = as<VarFn>(arg);
            if(f->getParamCount() < 1) {
                vm.fail(loc, "expected function to have at least 1",
                        " parameter for this option, found: ", f->getParamCount());
                return nullptr;
            }
        }
        VarClosure *cl = vm.makeVar<VarClosure>(loc, arg);
        for(size_t i = 3; i < args.size(); ++i) { cl->push(vm, args[i], true); }
        varCurl->setWriteCB(vm, cl);
        break;
    }
    case CURLOPT_HTTPHEADER: {
        EXPECT(VarMap, arg, "name-data pairs");
        curl_slist *lst = nullptr;
        if(!varCurl->createSList(vm, loc, as<VarMap>(arg), lst)) {
            vm.fail(loc, "failed to create slist from the given map");
            return nullptr;
        }
        if(lst) res = curl_easy_setopt(curl, (CURLoption)opt, lst);
        break;
    }
    // Ints
    case CURLOPT_HTTPAUTH:
    case CURLOPT_BUFFERSIZE:
    case CURLOPT_MAXFILESIZE:
    case CURLOPT_MAXREDIRS:
    case CURLOPT_PORT:
    case CURLOPT_POST:
    case CURLOPT_TCP_KEEPALIVE:
    case CURLOPT_CONNECT_ONLY:
    case CURLOPT_FOLLOWLOCATION:
    case CURLOPT_NOPROGRESS:
    case CURLOPT_NOBODY:
    case CURLOPT_VERBOSE: {
        EXPECT(VarInt, arg, "option value");
        res = curl_easy_setopt(curl, (CURLoption)opt, as<VarInt>(arg)->getVal());
        break;
    }
    // Strings
    case CURLOPT_POSTFIELDS: {
        // We don't want POSTFIELDS as it doesn't copy the string data to curl,
        // which is annoying to deal with.
        opt = CURLOPT_COPYPOSTFIELDS;
    }
    case CURLOPT_URL:
    case CURLOPT_PROXY:
    case CURLOPT_UNIX_SOCKET_PATH:
    case CURLOPT_DOH_URL:
    case CURLOPT_DNS_LOCAL_IP4:
    case CURLOPT_DNS_LOCAL_IP6:
    case CURLOPT_USERNAME:
    case CURLOPT_PASSWORD:
    case CURLOPT_USERPWD: // <user>:<password>
    case CURLOPT_LOGIN_OPTIONS:
    case CURLOPT_XOAUTH2_BEARER:
    case CURLOPT_SSLCERT:
    case CURLOPT_SSLCERTTYPE:
    case CURLOPT_SSLKEY:
    case CURLOPT_SSLKEYPASSWD:
    case CURLOPT_SSL_CIPHER_LIST:
    case CURLOPT_SSL_VERIFYHOST:
    case CURLOPT_SSL_VERIFYSTATUS:
    case CURLOPT_CAINFO:
    case CURLOPT_CAPATH:
    case CURLOPT_ISSUERCERT:
    case CURLOPT_PINNEDPUBLICKEY:
    case CURLOPT_SSH_HOST_PUBLIC_KEY_MD5:
    case CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256:
    case CURLOPT_SSH_PUBLIC_KEYFILE:
    case CURLOPT_SSH_PRIVATE_KEYFILE:
    case CURLOPT_SSH_KNOWNHOSTS:
    case CURLOPT_REFERER:
    case CURLOPT_COOKIE:
    case CURLOPT_COOKIEFILE:
    case CURLOPT_COOKIEJAR:
    case CURLOPT_COOKIELIST:
    case CURLOPT_MAIL_AUTH:
    case CURLOPT_MAIL_FROM:
    case CURLOPT_MAIL_RCPT:
    case CURLOPT_USERAGENT:
    case CURLOPT_CUSTOMREQUEST:
    case CURLOPT_COPYPOSTFIELDS: {
        EXPECT(VarStr, arg, "option value");
        res = curl_easy_setopt(curl, (CURLoption)opt, as<VarStr>(arg)->getVal().c_str());
        break;
    }
    default: {
        vm.fail(loc, "operation is not yet implemented");
        return nullptr;
    }
    }
    return vm.makeVar<VarInt>(loc, res);
}

FERAL_FUNC(curlEasySetProgressCBTick, 1, false,
           "  var.fn(tick) -> Nil\n"
           "Sets the interval in number of calls to "
           " progress callback where the callback does nothing.")
{
    EXPECT(VarInt, args[1], "tick count");
    VarCurlEasy *curl = as<VarCurlEasy>(args[0]);
    curl->setProgIntervalTickMax(as<VarInt>(args[1])->getVal());
    return vm.getNil();
}

FERAL_FUNC(curlEasyIsDone, 0, false,
           "  var.fn() -> Bool\n"
           "Returns `true` if the curl easy handle `var` is marked done.")
{
    VarCurlEasy *curl = as<VarCurlEasy>(args[0]);
    return curl->isDone() ? vm.getTrue() : vm.getFalse();
}

INIT_DLL(Curl)
{
    curl_global_init(CURL_GLOBAL_ALL);

    // Register the type names
    vm.addLocalType<VarCurlMulti>(loc, "CurlMulti", "Curl library's multi type representation.");
    vm.addLocalType<VarCurlEasy>(loc, "CurlEasy", "Curl library's easy type representation.");

    vm.addLocal(loc, "globalTrace", feralCurlGlobalTrace);
    vm.addLocal(loc, "multiStrErr", feralCurlMultiStrErr);
    vm.addLocal(loc, "easyStrErr", feralCurlEasyStrErr);
    vm.addLocal(loc, "newMulti", feralCurlMultiInit);
    vm.addLocal(loc, "newEasy", feralCurlEasyInit);

    vm.addTypeFn<VarCurlMulti>(loc, "addEasy", curlMultiAddEasy);
    vm.addTypeFn<VarCurlMulti>(loc, "perform", curlMultiPerform);
    vm.addTypeFn<VarCurlMulti>(loc, "setOptNative", curlMultiSetOptNative);
    vm.addTypeFn<VarCurlMulti>(loc, "empty", curlMultiEmpty);
    vm.addTypeFn<VarCurlMulti>(loc, "len", curlMultiLen);

    vm.addTypeFn<VarCurlEasy>(loc, "perform", curlEasyPerform);
    vm.addTypeFn<VarCurlEasy>(loc, "getHeader", curlEasyGetHeaderValue);
    vm.addTypeFn<VarCurlEasy>(loc, "getInfo", curlEasyGetInfo);
    vm.addTypeFn<VarCurlEasy>(loc, "setOptNative", curlEasySetOptNative);
    vm.addTypeFn<VarCurlEasy>(loc, "setProgressCBTickNative", curlEasySetProgressCBTick);
    vm.addTypeFn<VarCurlEasy>(loc, "done", curlEasyIsDone);

    setEnumVars(vm, loc);

    return true;
}

DEINIT_DLL(Curl) { curl_global_cleanup(); }

void setEnumVars(VirtualMachine &vm, ModuleLoc loc)
{
    // All the enum values

    // CURLcode
    vm.makeLocal<VarInt>(loc, "E_OK", "", CURLE_OK);
    vm.makeLocal<VarInt>(loc, "E_UNSUPPORTED_PROTOCOL", "", CURLE_UNSUPPORTED_PROTOCOL);
    vm.makeLocal<VarInt>(loc, "E_FAILED_INIT", "", CURLE_FAILED_INIT);
    vm.makeLocal<VarInt>(loc, "E_URL_MALFORMAT", "", CURLE_URL_MALFORMAT);
    vm.makeLocal<VarInt>(loc, "E_NOT_BUILT_IN", "", CURLE_NOT_BUILT_IN);
    vm.makeLocal<VarInt>(loc, "E_COULDNT_RESOLVE_PROXY", "", CURLE_COULDNT_RESOLVE_PROXY);
    vm.makeLocal<VarInt>(loc, "E_COULDNT_RESOLVE_HOST", "", CURLE_COULDNT_RESOLVE_HOST);
    vm.makeLocal<VarInt>(loc, "E_COULDNT_CONNECT", "", CURLE_COULDNT_CONNECT);
#if CURL_AT_LEAST_VERSION(7, 51, 0)
    vm.makeLocal<VarInt>(loc, "E_WEIRD_SERVER_REPLY", "", CURLE_WEIRD_SERVER_REPLY);
#else
    vm.makeLocal<VarInt>(loc, "E_FTP_WEIRD_SERVER_REPLY", "", CURLE_FTP_WEIRD_SERVER_REPLY);
#endif
    vm.makeLocal<VarInt>(loc, "E_REMOTE_ACCESS_DENIED", "", CURLE_REMOTE_ACCESS_DENIED);
    vm.makeLocal<VarInt>(loc, "E_FTP_ACCEPT_FAILED", "", CURLE_FTP_ACCEPT_FAILED);
    vm.makeLocal<VarInt>(loc, "E_FTP_WEIRD_PASS_REPLY", "", CURLE_FTP_WEIRD_PASS_REPLY);
    vm.makeLocal<VarInt>(loc, "E_FTP_ACCEPT_TIMEOUT", "", CURLE_FTP_ACCEPT_TIMEOUT);
    vm.makeLocal<VarInt>(loc, "E_FTP_WEIRD_PASV_REPLY", "", CURLE_FTP_WEIRD_PASV_REPLY);
    vm.makeLocal<VarInt>(loc, "E_FTP_WEIRD_227_FORMAT", "", CURLE_FTP_WEIRD_227_FORMAT);
    vm.makeLocal<VarInt>(loc, "E_FTP_CANT_GET_HOST", "", CURLE_FTP_CANT_GET_HOST);
    vm.makeLocal<VarInt>(loc, "E_HTTP2", "", CURLE_HTTP2);
    vm.makeLocal<VarInt>(loc, "E_FTP_COULDNT_SET_TYPE", "", CURLE_FTP_COULDNT_SET_TYPE);
    vm.makeLocal<VarInt>(loc, "E_PARTIAL_FILE", "", CURLE_PARTIAL_FILE);
    vm.makeLocal<VarInt>(loc, "E_FTP_COULDNT_RETR_FILE", "", CURLE_FTP_COULDNT_RETR_FILE);
    vm.makeLocal<VarInt>(loc, "E_OBSOLETE20", "", CURLE_OBSOLETE20);
    vm.makeLocal<VarInt>(loc, "E_QUOTE_ERROR", "", CURLE_QUOTE_ERROR);
    vm.makeLocal<VarInt>(loc, "E_HTTP_RETURNED_ERROR", "", CURLE_HTTP_RETURNED_ERROR);
    vm.makeLocal<VarInt>(loc, "E_WRITE_ERROR", "", CURLE_WRITE_ERROR);
    vm.makeLocal<VarInt>(loc, "E_OBSOLETE24", "", CURLE_OBSOLETE24);
    vm.makeLocal<VarInt>(loc, "E_UPLOAD_FAILED", "", CURLE_UPLOAD_FAILED);
    vm.makeLocal<VarInt>(loc, "E_READ_ERROR", "", CURLE_READ_ERROR);
    vm.makeLocal<VarInt>(loc, "E_OUT_OF_MEMORY", "", CURLE_OUT_OF_MEMORY);
    vm.makeLocal<VarInt>(loc, "E_OPERATION_TIMEDOUT", "", CURLE_OPERATION_TIMEDOUT);
    vm.makeLocal<VarInt>(loc, "E_OBSOLETE29", "", CURLE_OBSOLETE29);
    vm.makeLocal<VarInt>(loc, "E_FTP_PORT_FAILED", "", CURLE_FTP_PORT_FAILED);
    vm.makeLocal<VarInt>(loc, "E_FTP_COULDNT_USE_REST", "", CURLE_FTP_COULDNT_USE_REST);
    vm.makeLocal<VarInt>(loc, "E_OBSOLETE32", "", CURLE_OBSOLETE32);
    vm.makeLocal<VarInt>(loc, "E_RANGE_ERROR", "", CURLE_RANGE_ERROR);
    vm.makeLocal<VarInt>(loc, "E_HTTP_POST_ERROR", "", CURLE_HTTP_POST_ERROR);
    vm.makeLocal<VarInt>(loc, "E_SSL_CONNECT_ERROR", "", CURLE_SSL_CONNECT_ERROR);
    vm.makeLocal<VarInt>(loc, "E_BAD_DOWNLOAD_RESUME", "", CURLE_BAD_DOWNLOAD_RESUME);
    vm.makeLocal<VarInt>(loc, "E_FILE_COULDNT_READ_FILE", "", CURLE_FILE_COULDNT_READ_FILE);
    vm.makeLocal<VarInt>(loc, "E_LDAP_CANNOT_BIND", "", CURLE_LDAP_CANNOT_BIND);
    vm.makeLocal<VarInt>(loc, "E_LDAP_SEARCH_FAILED", "", CURLE_LDAP_SEARCH_FAILED);
    vm.makeLocal<VarInt>(loc, "E_OBSOLETE40", "", CURLE_OBSOLETE40);
    vm.makeLocal<VarInt>(loc, "E_FUNCTION_NOT_FOUND", "", CURLE_FUNCTION_NOT_FOUND);
    vm.makeLocal<VarInt>(loc, "E_ABORTED_BY_CALLBACK", "", CURLE_ABORTED_BY_CALLBACK);
    vm.makeLocal<VarInt>(loc, "E_BAD_FUNCTION_ARGUMENT", "", CURLE_BAD_FUNCTION_ARGUMENT);
    vm.makeLocal<VarInt>(loc, "E_OBSOLETE44", "", CURLE_OBSOLETE44);
    vm.makeLocal<VarInt>(loc, "E_INTERFACE_FAILED", "", CURLE_INTERFACE_FAILED);
    vm.makeLocal<VarInt>(loc, "E_OBSOLETE46", "", CURLE_OBSOLETE46);
    vm.makeLocal<VarInt>(loc, "E_TOO_MANY_REDIRECTS", "", CURLE_TOO_MANY_REDIRECTS);
    vm.makeLocal<VarInt>(loc, "E_UNKNOWN_OPTION", "", CURLE_UNKNOWN_OPTION);
    vm.makeLocal<VarInt>(loc, "E_TELNET_OPTION_SYNTAX", "", CURLE_TELNET_OPTION_SYNTAX);
    vm.makeLocal<VarInt>(loc, "E_OBSOLETE50", "", CURLE_OBSOLETE50);
#if CURL_AT_LEAST_VERSION(7, 62, 0)
    vm.makeLocal<VarInt>(loc, "E_OBSOLETE51", "", CURLE_OBSOLETE51);
#endif
    vm.makeLocal<VarInt>(loc, "E_GOT_NOTHING", "", CURLE_GOT_NOTHING);
    vm.makeLocal<VarInt>(loc, "E_SSL_ENGINE_NOTFOUND", "", CURLE_SSL_ENGINE_NOTFOUND);
    vm.makeLocal<VarInt>(loc, "E_SSL_ENGINE_SETFAILED", "", CURLE_SSL_ENGINE_SETFAILED);
    vm.makeLocal<VarInt>(loc, "E_SEND_ERROR", "", CURLE_SEND_ERROR);
    vm.makeLocal<VarInt>(loc, "E_RECV_ERROR", "", CURLE_RECV_ERROR);
    vm.makeLocal<VarInt>(loc, "E_OBSOLETE57", "", CURLE_OBSOLETE57);
    vm.makeLocal<VarInt>(loc, "E_SSL_CERTPROBLEM", "", CURLE_SSL_CERTPROBLEM);
    vm.makeLocal<VarInt>(loc, "E_SSL_CIPHER", "", CURLE_SSL_CIPHER);
    vm.makeLocal<VarInt>(loc, "E_PEER_FAILED_VERIFICATION", "", CURLE_PEER_FAILED_VERIFICATION);
    vm.makeLocal<VarInt>(loc, "E_BAD_CONTENT_ENCODING", "", CURLE_BAD_CONTENT_ENCODING);
    vm.makeLocal<VarInt>(loc, "E_LDAP_INVALID_URL", "", CURLE_LDAP_INVALID_URL);
    vm.makeLocal<VarInt>(loc, "E_FILESIZE_EXCEEDED", "", CURLE_FILESIZE_EXCEEDED);
    vm.makeLocal<VarInt>(loc, "E_USE_SSL_FAILED", "", CURLE_USE_SSL_FAILED);
    vm.makeLocal<VarInt>(loc, "E_SEND_FAIL_REWIND", "", CURLE_SEND_FAIL_REWIND);
    vm.makeLocal<VarInt>(loc, "E_SSL_ENGINE_INITFAILED", "", CURLE_SSL_ENGINE_INITFAILED);
    vm.makeLocal<VarInt>(loc, "E_LOGIN_DENIED", "", CURLE_LOGIN_DENIED);
    vm.makeLocal<VarInt>(loc, "E_TFTP_NOTFOUND", "", CURLE_TFTP_NOTFOUND);
    vm.makeLocal<VarInt>(loc, "E_TFTP_PERM", "", CURLE_TFTP_PERM);
    vm.makeLocal<VarInt>(loc, "E_REMOTE_DISK_FULL", "", CURLE_REMOTE_DISK_FULL);
    vm.makeLocal<VarInt>(loc, "E_TFTP_ILLEGAL", "", CURLE_TFTP_ILLEGAL);
    vm.makeLocal<VarInt>(loc, "E_TFTP_UNKNOWNID", "", CURLE_TFTP_UNKNOWNID);
    vm.makeLocal<VarInt>(loc, "E_REMOTE_FILE_EXISTS", "", CURLE_REMOTE_FILE_EXISTS);
    vm.makeLocal<VarInt>(loc, "E_TFTP_NOSUCHUSER", "", CURLE_TFTP_NOSUCHUSER);
    vm.makeLocal<VarInt>(loc, "E_CONV_FAILED", "", CURLE_CONV_FAILED);
    vm.makeLocal<VarInt>(loc, "E_CONV_REQD", "", CURLE_CONV_REQD);
    vm.makeLocal<VarInt>(loc, "E_SSL_CACERT_BADFILE", "", CURLE_SSL_CACERT_BADFILE);
    vm.makeLocal<VarInt>(loc, "E_REMOTE_FILE_NOT_FOUND", "", CURLE_REMOTE_FILE_NOT_FOUND);
    vm.makeLocal<VarInt>(loc, "E_SSH", "", CURLE_SSH);
    vm.makeLocal<VarInt>(loc, "E_SSL_SHUTDOWN_FAILED", "", CURLE_SSL_SHUTDOWN_FAILED);
    vm.makeLocal<VarInt>(loc, "E_AGAIN", "", CURLE_AGAIN);
    vm.makeLocal<VarInt>(loc, "E_SSL_CRL_BADFILE", "", CURLE_SSL_CRL_BADFILE);
    vm.makeLocal<VarInt>(loc, "E_SSL_ISSUER_ERROR", "", CURLE_SSL_ISSUER_ERROR);
    vm.makeLocal<VarInt>(loc, "E_FTP_PRET_FAILED", "", CURLE_FTP_PRET_FAILED);
    vm.makeLocal<VarInt>(loc, "E_RTSP_CSEQ_ERROR", "", CURLE_RTSP_CSEQ_ERROR);
    vm.makeLocal<VarInt>(loc, "E_RTSP_SESSION_ERROR", "", CURLE_RTSP_SESSION_ERROR);
    vm.makeLocal<VarInt>(loc, "E_FTP_BAD_FILE_LIST", "", CURLE_FTP_BAD_FILE_LIST);
    vm.makeLocal<VarInt>(loc, "E_CHUNK_FAILED", "", CURLE_CHUNK_FAILED);
    vm.makeLocal<VarInt>(loc, "E_NO_CONNECTION_AVAILABLE", "", CURLE_NO_CONNECTION_AVAILABLE);
    vm.makeLocal<VarInt>(loc, "E_SSL_PINNEDPUBKEYNOTMATCH", "", CURLE_SSL_PINNEDPUBKEYNOTMATCH);
    vm.makeLocal<VarInt>(loc, "E_SSL_INVALIDCERTSTATUS", "", CURLE_SSL_INVALIDCERTSTATUS);
#if CURL_AT_LEAST_VERSION(7, 50, 2)
    vm.makeLocal<VarInt>(loc, "E_HTTP2_STREAM", "", CURLE_HTTP2_STREAM);
#endif
#if CURL_AT_LEAST_VERSION(7, 59, 0)
    vm.makeLocal<VarInt>(loc, "E_RECURSIVE_API_CALL", "", CURLE_RECURSIVE_API_CALL);
#endif
#if CURL_AT_LEAST_VERSION(7, 66, 0)
    vm.makeLocal<VarInt>(loc, "E_AUTH_ERROR", "", CURLE_AUTH_ERROR);
#endif
#if CURL_AT_LEAST_VERSION(7, 68, 0)
    vm.makeLocal<VarInt>(loc, "E_HTTP3", "", CURLE_HTTP3);
#endif
#if CURL_AT_LEAST_VERSION(7, 69, 0)
    vm.makeLocal<VarInt>(loc, "E_QUIC_CONNECT_ERROR", "", CURLE_QUIC_CONNECT_ERROR);
#endif

    // CURLMcode
    vm.makeLocal<VarInt>(loc, "M_CALL_MULTI_PERFORM", "", CURLM_CALL_MULTI_PERFORM);
    vm.makeLocal<VarInt>(loc, "M_OK", "", CURLM_OK);
    vm.makeLocal<VarInt>(loc, "M_BAD_HANDLE", "", CURLM_BAD_HANDLE);
    vm.makeLocal<VarInt>(loc, "M_BAD_EASY_HANDLE", "", CURLM_BAD_EASY_HANDLE);
    vm.makeLocal<VarInt>(loc, "M_OUT_OF_MEMORY", "", CURLM_OUT_OF_MEMORY);
    vm.makeLocal<VarInt>(loc, "M_INTERNAL_ERROR", "", CURLM_INTERNAL_ERROR);
    vm.makeLocal<VarInt>(loc, "M_BAD_SOCKET", "", CURLM_BAD_SOCKET);
    vm.makeLocal<VarInt>(loc, "M_UNKNOWN_OPTION", "", CURLM_UNKNOWN_OPTION);
    vm.makeLocal<VarInt>(loc, "M_ADDED_ALREADY", "", CURLM_ADDED_ALREADY);
#if CURL_AT_LEAST_VERSION(7, 59, 0)
    vm.makeLocal<VarInt>(loc, "M_RECURSIVE_API_CALL", "", CURLM_RECURSIVE_API_CALL);
#endif
#if CURL_AT_LEAST_VERSION(7, 68, 0)
    vm.makeLocal<VarInt>(loc, "WAKEUP_FAILURE", "", CURLM_WAKEUP_FAILURE);
#endif
#if CURL_AT_LEAST_VERSION(7, 69, 0)
    vm.makeLocal<VarInt>(loc, "BAD_FUNCTION_ARGUMENT", "", CURLM_BAD_FUNCTION_ARGUMENT);
#endif

    // CURLSHcode
    vm.makeLocal<VarInt>(loc, "SHE_OK", "", CURLSHE_OK);
    vm.makeLocal<VarInt>(loc, "SHE_BAD_OPTION", "", CURLSHE_BAD_OPTION);
    vm.makeLocal<VarInt>(loc, "SHE_IN_USE", "", CURLSHE_IN_USE);
    vm.makeLocal<VarInt>(loc, "SHE_INVALID", "", CURLSHE_INVALID);
    vm.makeLocal<VarInt>(loc, "SHE_NOMEM", "", CURLSHE_NOMEM);
    vm.makeLocal<VarInt>(loc, "SHE_NOT_BUILT_IN", "", CURLSHE_NOT_BUILT_IN);

#if CURL_AT_LEAST_VERSION(7, 62, 0)
    // CURLUcode
    vm.makeLocal<VarInt>(loc, "UE_OK", "", CURLUE_OK);
    vm.makeLocal<VarInt>(loc, "UE_BAD_HANDLE", "", CURLUE_BAD_HANDLE);
    vm.makeLocal<VarInt>(loc, "UE_BAD_PARTPOINTER", "", CURLUE_BAD_PARTPOINTER);
    vm.makeLocal<VarInt>(loc, "UE_MALFORMED_INPUT", "", CURLUE_MALFORMED_INPUT);
    vm.makeLocal<VarInt>(loc, "UE_BAD_PORT_NUMBER", "", CURLUE_BAD_PORT_NUMBER);
    vm.makeLocal<VarInt>(loc, "UE_UNSUPPORTED_SCHEME", "", CURLUE_UNSUPPORTED_SCHEME);
    vm.makeLocal<VarInt>(loc, "UE_URLDECODE", "", CURLUE_URLDECODE);
    vm.makeLocal<VarInt>(loc, "UE_OUT_OF_MEMORY", "", CURLUE_OUT_OF_MEMORY);
    vm.makeLocal<VarInt>(loc, "UE_USER_NOT_ALLOWED", "", CURLUE_USER_NOT_ALLOWED);
    vm.makeLocal<VarInt>(loc, "UE_UNKNOWN_PART", "", CURLUE_UNKNOWN_PART);
    vm.makeLocal<VarInt>(loc, "UE_NO_SCHEME", "", CURLUE_NO_SCHEME);
    vm.makeLocal<VarInt>(loc, "UE_NO_USER", "", CURLUE_NO_USER);
    vm.makeLocal<VarInt>(loc, "UE_NO_PASSWORD", "", CURLUE_NO_PASSWORD);
    vm.makeLocal<VarInt>(loc, "UE_NO_OPTIONS", "", CURLUE_NO_OPTIONS);
    vm.makeLocal<VarInt>(loc, "UE_NO_HOST", "", CURLUE_NO_HOST);
    vm.makeLocal<VarInt>(loc, "UE_NO_PORT", "", CURLUE_NO_PORT);
    vm.makeLocal<VarInt>(loc, "UE_NO_QUERY", "", CURLUE_NO_QUERY);
    vm.makeLocal<VarInt>(loc, "UE_NO_FRAGMENT", "", CURLUE_NO_FRAGMENT);
#endif

    // EASY_OPTS

    // BEHAVIOR OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_VERBOSE", "", CURLOPT_VERBOSE);
    vm.makeLocal<VarInt>(loc, "OPT_HEADER", "", CURLOPT_HEADER);
    vm.makeLocal<VarInt>(loc, "OPT_NOPROGRESS", "", CURLOPT_NOPROGRESS);
    vm.makeLocal<VarInt>(loc, "OPT_NOSIGNAL", "", CURLOPT_NOSIGNAL);
    vm.makeLocal<VarInt>(loc, "OPT_WILDCARDMATCH", "", CURLOPT_WILDCARDMATCH);

    // CALLBACK OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_WRITEFUNCTION", "", CURLOPT_WRITEFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_WRITEDATA", "", CURLOPT_WRITEDATA);
    vm.makeLocal<VarInt>(loc, "OPT_READFUNCTION", "", CURLOPT_READFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_READDATA", "", CURLOPT_READDATA);
    vm.makeLocal<VarInt>(loc, "OPT_SEEKFUNCTION", "", CURLOPT_SEEKFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_SEEKDATA", "", CURLOPT_SEEKDATA);
    vm.makeLocal<VarInt>(loc, "OPT_SOCKOPTFUNCTION", "", CURLOPT_SOCKOPTFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_SOCKOPTDATA", "", CURLOPT_SOCKOPTDATA);
    vm.makeLocal<VarInt>(loc, "OPT_OPENSOCKETFUNCTION", "", CURLOPT_OPENSOCKETFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_OPENSOCKETDATA", "", CURLOPT_OPENSOCKETDATA);
    vm.makeLocal<VarInt>(loc, "OPT_CLOSESOCKETFUNCTION", "", CURLOPT_CLOSESOCKETFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_CLOSESOCKETDATA", "", CURLOPT_CLOSESOCKETDATA);
    vm.makeLocal<VarInt>(loc, "OPT_PROGRESSDATA", "", CURLOPT_PROGRESSDATA);
    vm.makeLocal<VarInt>(loc, "OPT_XFERINFOFUNCTION", "", CURLOPT_XFERINFOFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_XFERINFODATA", "", CURLOPT_XFERINFODATA);
    vm.makeLocal<VarInt>(loc, "OPT_HEADERFUNCTION", "", CURLOPT_HEADERFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_HEADERDATA", "", CURLOPT_HEADERDATA);
    vm.makeLocal<VarInt>(loc, "OPT_DEBUGFUNCTION", "", CURLOPT_DEBUGFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_DEBUGDATA", "", CURLOPT_DEBUGDATA);
    vm.makeLocal<VarInt>(loc, "OPT_SSL_CTX_FUNCTION", "", CURLOPT_SSL_CTX_FUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_SSL_CTX_DATA", "", CURLOPT_SSL_CTX_DATA);
    vm.makeLocal<VarInt>(loc, "OPT_INTERLEAVEFUNCTION", "", CURLOPT_INTERLEAVEFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_INTERLEAVEDATA", "", CURLOPT_INTERLEAVEDATA);
    vm.makeLocal<VarInt>(loc, "OPT_CHUNK_BGN_FUNCTION", "", CURLOPT_CHUNK_BGN_FUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_CHUNK_END_FUNCTION", "", CURLOPT_CHUNK_END_FUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_CHUNK_DATA", "", CURLOPT_CHUNK_DATA);
    vm.makeLocal<VarInt>(loc, "OPT_FNMATCH_FUNCTION", "", CURLOPT_FNMATCH_FUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_FNMATCH_DATA", "", CURLOPT_FNMATCH_DATA);
#if CURL_AT_LEAST_VERSION(7, 54, 0)
    vm.makeLocal<VarInt>(loc, "OPT_SUPPRESS_CONNECT_HEADERS", "", CURLOPT_SUPPRESS_CONNECT_HEADERS);
#endif
#if CURL_AT_LEAST_VERSION(7, 59, 0)
    vm.makeLocal<VarInt>(loc, "OPT_RESOLVER_START_FUNCTION", "", CURLOPT_RESOLVER_START_FUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_RESOLVER_START_DATA", "", CURLOPT_RESOLVER_START_DATA);
#endif

    // ERROR OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_ERRORBUFFER", "", CURLOPT_ERRORBUFFER);
    vm.makeLocal<VarInt>(loc, "OPT_STDERR", "", CURLOPT_STDERR);
    vm.makeLocal<VarInt>(loc, "OPT_FAILONERROR", "", CURLOPT_FAILONERROR);
#if CURL_AT_LEAST_VERSION(7, 51, 0)
    vm.makeLocal<VarInt>(loc, "OPT_KEEP_SENDING_ON_ERROR", "", CURLOPT_KEEP_SENDING_ON_ERROR);
#endif

    // NETWORK OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_URL", "", CURLOPT_URL);
    vm.makeLocal<VarInt>(loc, "OPT_PATH_AS_IS", "", CURLOPT_PATH_AS_IS);
    vm.makeLocal<VarInt>(loc, "OPT_PROTOCOLS_STR", "", CURLOPT_PROTOCOLS_STR);
    vm.makeLocal<VarInt>(loc, "OPT_REDIR_PROTOCOLS_STR", "", CURLOPT_REDIR_PROTOCOLS_STR);
    vm.makeLocal<VarInt>(loc, "OPT_DEFAULT_PROTOCOL", "", CURLOPT_DEFAULT_PROTOCOL);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY", "", CURLOPT_PROXY);
#if CURL_AT_LEAST_VERSION(7, 52, 0)
    vm.makeLocal<VarInt>(loc, "OPT_PRE_PROXY", "", CURLOPT_PRE_PROXY);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_PROXYPORT", "", CURLOPT_PROXYPORT);
    vm.makeLocal<VarInt>(loc, "OPT_PROXYTYPE", "", CURLOPT_PROXYTYPE);
    vm.makeLocal<VarInt>(loc, "OPT_NOPROXY", "", CURLOPT_NOPROXY);
    vm.makeLocal<VarInt>(loc, "OPT_HTTPPROXYTUNNEL", "", CURLOPT_HTTPPROXYTUNNEL);
#if CURL_AT_LEAST_VERSION(7, 49, 0)
    vm.makeLocal<VarInt>(loc, "OPT_CONNECT_TO", "", CURLOPT_CONNECT_TO);
#endif
#if CURL_AT_LEAST_VERSION(7, 55, 0)
    vm.makeLocal<VarInt>(loc, "OPT_SOCKS5_AUTH", "", CURLOPT_SOCKS5_AUTH);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_SOCKS5_GSSAPI_NEC", "", CURLOPT_SOCKS5_GSSAPI_NEC);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_SERVICE_NAME", "", CURLOPT_PROXY_SERVICE_NAME);
#if CURL_AT_LEAST_VERSION(7, 60, 0)
    vm.makeLocal<VarInt>(loc, "OPT_HAPROXYPROTOCOL", "", CURLOPT_HAPROXYPROTOCOL);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_SERVICE_NAME", "", CURLOPT_SERVICE_NAME);
    vm.makeLocal<VarInt>(loc, "OPT_INTERFACE", "", CURLOPT_INTERFACE);
    vm.makeLocal<VarInt>(loc, "OPT_LOCALPORT", "", CURLOPT_LOCALPORT);
    vm.makeLocal<VarInt>(loc, "OPT_LOCALPORTRANGE", "", CURLOPT_LOCALPORTRANGE);
    vm.makeLocal<VarInt>(loc, "OPT_DNS_CACHE_TIMEOUT", "", CURLOPT_DNS_CACHE_TIMEOUT);
#if CURL_AT_LEAST_VERSION(7, 62, 0)
    vm.makeLocal<VarInt>(loc, "OPT_DOH_URL", "", CURLOPT_DOH_URL);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_BUFFERSIZE", "", CURLOPT_BUFFERSIZE);
    vm.makeLocal<VarInt>(loc, "OPT_PORT", "", CURLOPT_PORT);
#if CURL_AT_LEAST_VERSION(7, 49, 0)
    vm.makeLocal<VarInt>(loc, "OPT_TCP_FASTOPEN", "", CURLOPT_TCP_FASTOPEN);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_TCP_NODELAY", "", CURLOPT_TCP_NODELAY);
    vm.makeLocal<VarInt>(loc, "OPT_ADDRESS_SCOPE", "", CURLOPT_ADDRESS_SCOPE);
    vm.makeLocal<VarInt>(loc, "OPT_TCP_KEEPALIVE", "", CURLOPT_TCP_KEEPALIVE);
    vm.makeLocal<VarInt>(loc, "OPT_TCP_KEEPIDLE", "", CURLOPT_TCP_KEEPIDLE);
    vm.makeLocal<VarInt>(loc, "OPT_TCP_KEEPINTVL", "", CURLOPT_TCP_KEEPINTVL);
    vm.makeLocal<VarInt>(loc, "OPT_UNIX_SOCKET_PATH", "", CURLOPT_UNIX_SOCKET_PATH);
#if CURL_AT_LEAST_VERSION(7, 53, 0)
    vm.makeLocal<VarInt>(loc, "OPT_ABSTRACT_UNIX_SOCKET", "", CURLOPT_ABSTRACT_UNIX_SOCKET);
#endif

    // NAMES and PASSWORDS OPTIONS (Authentication)
    vm.makeLocal<VarInt>(loc, "OPT_NETRC", "", CURLOPT_NETRC);
    vm.makeLocal<VarInt>(loc, "OPT_NETRC_FILE", "", CURLOPT_NETRC_FILE);
    vm.makeLocal<VarInt>(loc, "OPT_USERPWD", "", CURLOPT_USERPWD);
    vm.makeLocal<VarInt>(loc, "OPT_PROXYUSERPWD", "", CURLOPT_PROXYUSERPWD);
    vm.makeLocal<VarInt>(loc, "OPT_USERNAME", "", CURLOPT_USERNAME);
    vm.makeLocal<VarInt>(loc, "OPT_PASSWORD", "", CURLOPT_PASSWORD);
    vm.makeLocal<VarInt>(loc, "OPT_LOGIN_OPTIONS", "", CURLOPT_LOGIN_OPTIONS);
    vm.makeLocal<VarInt>(loc, "OPT_PROXYUSERNAME", "", CURLOPT_PROXYUSERNAME);
    vm.makeLocal<VarInt>(loc, "OPT_PROXYPASSWORD", "", CURLOPT_PROXYPASSWORD);
    vm.makeLocal<VarInt>(loc, "OPT_HTTPAUTH", "", CURLOPT_HTTPAUTH);
    vm.makeLocal<VarInt>(loc, "OPT_TLSAUTH_USERNAME", "", CURLOPT_TLSAUTH_USERNAME);
    vm.makeLocal<VarInt>(loc, "OPT_TLSAUTH_PASSWORD", "", CURLOPT_TLSAUTH_PASSWORD);
    vm.makeLocal<VarInt>(loc, "OPT_TLSAUTH_TYPE", "", CURLOPT_TLSAUTH_TYPE);
#if CURL_AT_LEAST_VERSION(7, 52, 0)
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_TLSAUTH_USERNAME", "", CURLOPT_PROXY_TLSAUTH_USERNAME);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_TLSAUTH_PASSWORD", "", CURLOPT_PROXY_TLSAUTH_PASSWORD);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_TLSAUTH_TYPE", "", CURLOPT_PROXY_TLSAUTH_TYPE);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_PROXYAUTH", "", CURLOPT_PROXYAUTH);
#if CURL_AT_LEAST_VERSION(7, 66, 0)
    vm.makeLocal<VarInt>(loc, "OPT_SASL_AUTHZID", "", CURLOPT_SASL_AUTHZID);
#endif
#if CURL_AT_LEAST_VERSION(7, 61, 0)
    vm.makeLocal<VarInt>(loc, "OPT_SASL_IR", "", CURLOPT_SASL_IR);
    vm.makeLocal<VarInt>(loc, "OPT_DISALLOW_USERNAME_IN_URL", "", CURLOPT_DISALLOW_USERNAME_IN_URL);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_XOAUTH2_BEARER", "", CURLOPT_XOAUTH2_BEARER);

    // HTTP OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_AUTOREFERER", "", CURLOPT_AUTOREFERER);
    vm.makeLocal<VarInt>(loc, "OPT_ACCEPT_ENCODING", "", CURLOPT_ACCEPT_ENCODING);
    vm.makeLocal<VarInt>(loc, "OPT_TRANSFER_ENCODING", "", CURLOPT_TRANSFER_ENCODING);
    vm.makeLocal<VarInt>(loc, "OPT_FOLLOWLOCATION", "", CURLOPT_FOLLOWLOCATION);
    vm.makeLocal<VarInt>(loc, "OPT_UNRESTRICTED_AUTH", "", CURLOPT_UNRESTRICTED_AUTH);
    vm.makeLocal<VarInt>(loc, "OPT_MAXREDIRS", "", CURLOPT_MAXREDIRS);
    vm.makeLocal<VarInt>(loc, "OPT_POSTREDIR", "", CURLOPT_POSTREDIR);
    vm.makeLocal<VarInt>(loc, "OPT_POST", "", CURLOPT_POST);
    vm.makeLocal<VarInt>(loc, "OPT_POSTFIELDS", "", CURLOPT_POSTFIELDS);
    vm.makeLocal<VarInt>(loc, "OPT_POSTFIELDSIZE", "", CURLOPT_POSTFIELDSIZE);
    vm.makeLocal<VarInt>(loc, "OPT_POSTFIELDSIZE_LARGE", "", CURLOPT_POSTFIELDSIZE_LARGE);
    vm.makeLocal<VarInt>(loc, "OPT_COPYPOSTFIELDS", "", CURLOPT_COPYPOSTFIELDS);
    vm.makeLocal<VarInt>(loc, "OPT_REFERER", "", CURLOPT_REFERER);
    vm.makeLocal<VarInt>(loc, "OPT_USERAGENT", "", CURLOPT_USERAGENT);
    vm.makeLocal<VarInt>(loc, "OPT_HTTPHEADER", "", CURLOPT_HTTPHEADER);
    vm.makeLocal<VarInt>(loc, "OPT_HEADEROPT", "", CURLOPT_HEADEROPT);
    vm.makeLocal<VarInt>(loc, "OPT_PROXYHEADER", "", CURLOPT_PROXYHEADER);
    vm.makeLocal<VarInt>(loc, "OPT_HTTP200ALIASES", "", CURLOPT_HTTP200ALIASES);
    vm.makeLocal<VarInt>(loc, "OPT_COOKIE", "", CURLOPT_COOKIE);
    vm.makeLocal<VarInt>(loc, "OPT_COOKIEFILE", "", CURLOPT_COOKIEFILE);
    vm.makeLocal<VarInt>(loc, "OPT_COOKIEJAR", "", CURLOPT_COOKIEJAR);
    vm.makeLocal<VarInt>(loc, "OPT_COOKIESESSION", "", CURLOPT_COOKIESESSION);
    vm.makeLocal<VarInt>(loc, "OPT_COOKIELIST", "", CURLOPT_COOKIELIST);
#if CURL_AT_LEAST_VERSION(7, 64, 1)
    vm.makeLocal<VarInt>(loc, "OPT_ALTSVC", "", CURLOPT_ALTSVC);
    vm.makeLocal<VarInt>(loc, "OPT_ALTSVC_CTRL", "", CURLOPT_ALTSVC_CTRL);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_HTTPGET", "", CURLOPT_HTTPGET);
#if CURL_AT_LEAST_VERSION(7, 55, 0)
    vm.makeLocal<VarInt>(loc, "OPT_REQUEST_TARGET", "", CURLOPT_REQUEST_TARGET);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_HTTP_VERSION", "", CURLOPT_HTTP_VERSION);
#if CURL_AT_LEAST_VERSION(7, 64, 0)
    vm.makeLocal<VarInt>(loc, "OPT_HTTP09_ALLOWED", "", CURLOPT_HTTP09_ALLOWED);
    vm.makeLocal<VarInt>(loc, "OPT_TRAILERFUNCTION", "", CURLOPT_TRAILERFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_TRAILERDATA", "", CURLOPT_TRAILERDATA);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_IGNORE_CONTENT_LENGTH", "", CURLOPT_IGNORE_CONTENT_LENGTH);
    vm.makeLocal<VarInt>(loc, "OPT_HTTP_CONTENT_DECODING", "", CURLOPT_HTTP_CONTENT_DECODING);
    vm.makeLocal<VarInt>(loc, "OPT_HTTP_TRANSFER_DECODING", "", CURLOPT_HTTP_TRANSFER_DECODING);
    vm.makeLocal<VarInt>(loc, "OPT_EXPECT_100_TIMEOUT_MS", "", CURLOPT_EXPECT_100_TIMEOUT_MS);
    vm.makeLocal<VarInt>(loc, "OPT_PIPEWAIT", "", CURLOPT_PIPEWAIT);
    vm.makeLocal<VarInt>(loc, "OPT_STREAM_DEPENDS", "", CURLOPT_STREAM_DEPENDS);
    vm.makeLocal<VarInt>(loc, "OPT_STREAM_DEPENDS_E", "", CURLOPT_STREAM_DEPENDS_E);
    vm.makeLocal<VarInt>(loc, "OPT_STREAM_WEIGHT", "", CURLOPT_STREAM_WEIGHT);

    // SMTP OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_MAIL_FROM", "", CURLOPT_MAIL_FROM);
    vm.makeLocal<VarInt>(loc, "OPT_MAIL_RCPT", "", CURLOPT_MAIL_RCPT);
    vm.makeLocal<VarInt>(loc, "OPT_MAIL_AUTH", "", CURLOPT_MAIL_AUTH);
#if CURL_AT_LEAST_VERSION(7, 69, 0)
    vm.makeLocal<VarInt>(loc, "OPT_MAIL_RCPT_ALLLOWFAILS", "", CURLOPT_MAIL_RCPT_ALLLOWFAILS);
#endif

    // TFTP OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_TFTP_BLKSIZE", "", CURLOPT_TFTP_BLKSIZE);
#if CURL_AT_LEAST_VERSION(7, 48, 0)
    vm.makeLocal<VarInt>(loc, "OPT_TFTP_NO_OPTIONS", "", CURLOPT_TFTP_NO_OPTIONS);
#endif

    // FTP OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_FTPPORT", "", CURLOPT_FTPPORT);
    vm.makeLocal<VarInt>(loc, "OPT_QUOTE", "", CURLOPT_QUOTE);
    vm.makeLocal<VarInt>(loc, "OPT_POSTQUOTE", "", CURLOPT_POSTQUOTE);
    vm.makeLocal<VarInt>(loc, "OPT_PREQUOTE", "", CURLOPT_PREQUOTE);
    vm.makeLocal<VarInt>(loc, "OPT_APPEND", "", CURLOPT_APPEND);
    vm.makeLocal<VarInt>(loc, "OPT_FTP_USE_EPRT", "", CURLOPT_FTP_USE_EPRT);
    vm.makeLocal<VarInt>(loc, "OPT_FTP_USE_EPSV", "", CURLOPT_FTP_USE_EPSV);
    vm.makeLocal<VarInt>(loc, "OPT_FTP_USE_PRET", "", CURLOPT_FTP_USE_PRET);
    vm.makeLocal<VarInt>(loc, "OPT_FTP_CREATE_MISSING_DIRS", "", CURLOPT_FTP_CREATE_MISSING_DIRS);
    vm.makeLocal<VarInt>(loc, "OPT_FTP_RESPONSE_TIMEOUT", "", CURLOPT_FTP_RESPONSE_TIMEOUT);
    vm.makeLocal<VarInt>(loc, "OPT_FTP_ALTERNATIVE_TO_USER", "", CURLOPT_FTP_ALTERNATIVE_TO_USER);
    vm.makeLocal<VarInt>(loc, "OPT_FTP_SKIP_PASV_IP", "", CURLOPT_FTP_SKIP_PASV_IP);
    vm.makeLocal<VarInt>(loc, "OPT_FTPSSLAUTH", "", CURLOPT_FTPSSLAUTH);
    vm.makeLocal<VarInt>(loc, "OPT_FTP_SSL_CCC", "", CURLOPT_FTP_SSL_CCC);
    vm.makeLocal<VarInt>(loc, "OPT_FTP_ACCOUNT", "", CURLOPT_FTP_ACCOUNT);
    vm.makeLocal<VarInt>(loc, "OPT_FTP_FILEMETHOD", "", CURLOPT_FTP_FILEMETHOD);

    // RTSP OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_RTSP_REQUEST", "", CURLOPT_RTSP_REQUEST);
    vm.makeLocal<VarInt>(loc, "OPT_RTSP_SESSION_ID", "", CURLOPT_RTSP_SESSION_ID);
    vm.makeLocal<VarInt>(loc, "OPT_RTSP_STREAM_URI", "", CURLOPT_RTSP_STREAM_URI);
    vm.makeLocal<VarInt>(loc, "OPT_RTSP_TRANSPORT", "", CURLOPT_RTSP_TRANSPORT);
    vm.makeLocal<VarInt>(loc, "OPT_RTSP_CLIENT_CSEQ", "", CURLOPT_RTSP_CLIENT_CSEQ);
    vm.makeLocal<VarInt>(loc, "OPT_RTSP_SERVER_CSEQ", "", CURLOPT_RTSP_SERVER_CSEQ);

    // PROTOCOL OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_TRANSFERTEXT", "", CURLOPT_TRANSFERTEXT);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_TRANSFER_MODE", "", CURLOPT_PROXY_TRANSFER_MODE);
    vm.makeLocal<VarInt>(loc, "OPT_CRLF", "", CURLOPT_CRLF);
    vm.makeLocal<VarInt>(loc, "OPT_RANGE", "", CURLOPT_RANGE);
    vm.makeLocal<VarInt>(loc, "OPT_RESUME_FROM", "", CURLOPT_RESUME_FROM);
    vm.makeLocal<VarInt>(loc, "OPT_RESUME_FROM_LARGE", "", CURLOPT_RESUME_FROM_LARGE);
#if CURL_AT_LEAST_VERSION(7, 63, 0)
    vm.makeLocal<VarInt>(loc, "OPT_CURLU", "", CURLOPT_CURLU);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_CUSTOMREQUEST", "", CURLOPT_CUSTOMREQUEST);
    vm.makeLocal<VarInt>(loc, "OPT_FILETIME", "", CURLOPT_FILETIME);
    vm.makeLocal<VarInt>(loc, "OPT_DIRLISTONLY", "", CURLOPT_DIRLISTONLY);
    vm.makeLocal<VarInt>(loc, "OPT_NOBODY", "", CURLOPT_NOBODY);
    vm.makeLocal<VarInt>(loc, "OPT_INFILESIZE", "", CURLOPT_INFILESIZE);
    vm.makeLocal<VarInt>(loc, "OPT_INFILESIZE_LARGE", "", CURLOPT_INFILESIZE_LARGE);
    vm.makeLocal<VarInt>(loc, "OPT_UPLOAD", "", CURLOPT_UPLOAD);
#if CURL_AT_LEAST_VERSION(7, 62, 0)
    vm.makeLocal<VarInt>(loc, "OPT_UPLOAD_BUFFERSIZE", "", CURLOPT_UPLOAD_BUFFERSIZE);
#endif
#if CURL_AT_LEAST_VERSION(7, 56, 0)
    vm.makeLocal<VarInt>(loc, "OPT_MIMEPOST", "", CURLOPT_MIMEPOST);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_MAXFILESIZE", "", CURLOPT_MAXFILESIZE);
    vm.makeLocal<VarInt>(loc, "OPT_MAXFILESIZE_LARGE", "", CURLOPT_MAXFILESIZE_LARGE);
    vm.makeLocal<VarInt>(loc, "OPT_TIMECONDITION", "", CURLOPT_TIMECONDITION);
    vm.makeLocal<VarInt>(loc, "OPT_TIMEVALUE", "", CURLOPT_TIMEVALUE);
#if CURL_AT_LEAST_VERSION(7, 59, 0)
    vm.makeLocal<VarInt>(loc, "OPT_TIMEVALUE_LARGE", "", CURLOPT_TIMEVALUE_LARGE);
#endif

    // CONNECTION OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_TIMEOUT", "", CURLOPT_TIMEOUT);
    vm.makeLocal<VarInt>(loc, "OPT_TIMEOUT_MS", "", CURLOPT_TIMEOUT_MS);
    vm.makeLocal<VarInt>(loc, "OPT_LOW_SPEED_LIMIT", "", CURLOPT_LOW_SPEED_LIMIT);
    vm.makeLocal<VarInt>(loc, "OPT_LOW_SPEED_TIME", "", CURLOPT_LOW_SPEED_TIME);
    vm.makeLocal<VarInt>(loc, "OPT_MAX_SEND_SPEED_LARGE", "", CURLOPT_MAX_SEND_SPEED_LARGE);
    vm.makeLocal<VarInt>(loc, "OPT_MAX_RECV_SPEED_LARGE", "", CURLOPT_MAX_RECV_SPEED_LARGE);
    vm.makeLocal<VarInt>(loc, "OPT_MAXCONNECTS", "", CURLOPT_MAXCONNECTS);
    vm.makeLocal<VarInt>(loc, "OPT_FRESH_CONNECT", "", CURLOPT_FRESH_CONNECT);
    vm.makeLocal<VarInt>(loc, "OPT_FORBID_REUSE", "", CURLOPT_FORBID_REUSE);
#if CURL_AT_LEAST_VERSION(7, 65, 0)
    vm.makeLocal<VarInt>(loc, "OPT_MAXAGE_CONN", "", CURLOPT_MAXAGE_CONN);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_CONNECTTIMEOUT", "", CURLOPT_CONNECTTIMEOUT);
    vm.makeLocal<VarInt>(loc, "OPT_CONNECTTIMEOUT_MS", "", CURLOPT_CONNECTTIMEOUT_MS);
    vm.makeLocal<VarInt>(loc, "OPT_IPRESOLVE", "", CURLOPT_IPRESOLVE);
    vm.makeLocal<VarInt>(loc, "OPT_CONNECT_ONLY", "", CURLOPT_CONNECT_ONLY);
    vm.makeLocal<VarInt>(loc, "OPT_USE_SSL", "", CURLOPT_USE_SSL);
    vm.makeLocal<VarInt>(loc, "OPT_RESOLVE", "", CURLOPT_RESOLVE);
    vm.makeLocal<VarInt>(loc, "OPT_DNS_INTERFACE", "", CURLOPT_DNS_INTERFACE);
    vm.makeLocal<VarInt>(loc, "OPT_DNS_LOCAL_IP4", "", CURLOPT_DNS_LOCAL_IP4);
    vm.makeLocal<VarInt>(loc, "OPT_DNS_LOCAL_IP6", "", CURLOPT_DNS_LOCAL_IP6);
    vm.makeLocal<VarInt>(loc, "OPT_DNS_SERVERS", "", CURLOPT_DNS_SERVERS);
#if CURL_AT_LEAST_VERSION(7, 60, 0)
    vm.makeLocal<VarInt>(loc, "OPT_DNS_SHUFFLE_ADDRESSES", "", CURLOPT_DNS_SHUFFLE_ADDRESSES);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_ACCEPTTIMEOUT_MS", "", CURLOPT_ACCEPTTIMEOUT_MS);
#if CURL_AT_LEAST_VERSION(7, 59, 0)
    vm.makeLocal<VarInt>(loc, "OPT_HAPPY_EYEBALLS_TIMEOUT_MS", "",
                         CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS);
#endif
#if CURL_AT_LEAST_VERSION(7, 62, 0)
    vm.makeLocal<VarInt>(loc, "OPT_UPKEEP_INTERVAL_MS", "", CURLOPT_UPKEEP_INTERVAL_MS);
#endif

    // SSL and SECURITY OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_SSLCERT", "", CURLOPT_SSLCERT);
    vm.makeLocal<VarInt>(loc, "OPT_SSLCERTTYPE", "", CURLOPT_SSLCERTTYPE);
    vm.makeLocal<VarInt>(loc, "OPT_SSLKEY", "", CURLOPT_SSLKEY);
    vm.makeLocal<VarInt>(loc, "OPT_SSLKEYTYPE", "", CURLOPT_SSLKEYTYPE);
    vm.makeLocal<VarInt>(loc, "OPT_KEYPASSWD", "", CURLOPT_KEYPASSWD);
    vm.makeLocal<VarInt>(loc, "OPT_SSL_ENABLE_ALPN", "", CURLOPT_SSL_ENABLE_ALPN);
    vm.makeLocal<VarInt>(loc, "OPT_SSLENGINE", "", CURLOPT_SSLENGINE);
    vm.makeLocal<VarInt>(loc, "OPT_SSLENGINE_DEFAULT", "", CURLOPT_SSLENGINE_DEFAULT);
    vm.makeLocal<VarInt>(loc, "OPT_SSLVERSION", "", CURLOPT_SSLVERSION);
    vm.makeLocal<VarInt>(loc, "OPT_SSL_VERIFYPEER", "", CURLOPT_SSL_VERIFYPEER);
    vm.makeLocal<VarInt>(loc, "OPT_SSL_VERIFYHOST", "", CURLOPT_SSL_VERIFYHOST);
    vm.makeLocal<VarInt>(loc, "OPT_SSL_VERIFYSTATUS", "", CURLOPT_SSL_VERIFYSTATUS);
#if CURL_AT_LEAST_VERSION(7, 52, 0)
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_CAINFO", "", CURLOPT_PROXY_CAINFO);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_CAPATH", "", CURLOPT_PROXY_CAPATH);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_CRLFILE", "", CURLOPT_PROXY_CRLFILE);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_KEYPASSWD", "", CURLOPT_PROXY_KEYPASSWD);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_PINNEDPUBLICKEY", "", CURLOPT_PROXY_PINNEDPUBLICKEY);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_SSLCERT", "", CURLOPT_PROXY_SSLCERT);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_SSLCERTTYPE", "", CURLOPT_PROXY_SSLCERTTYPE);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_SSLKEY", "", CURLOPT_PROXY_SSLKEY);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_SSLKEYTYPE", "", CURLOPT_PROXY_SSLKEYTYPE);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_SSLVERSION", "", CURLOPT_PROXY_SSLVERSION);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_SSL_CIPHER_LIST", "", CURLOPT_PROXY_SSL_CIPHER_LIST);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_SSL_OPTIONS", "", CURLOPT_PROXY_SSL_OPTIONS);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_SSL_VERIFYHOST", "", CURLOPT_PROXY_SSL_VERIFYHOST);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_SSL_VERIFYPEER", "", CURLOPT_PROXY_SSL_VERIFYPEER);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_CAINFO", "", CURLOPT_CAINFO);
    vm.makeLocal<VarInt>(loc, "OPT_ISSUERCERT", "", CURLOPT_ISSUERCERT);
    vm.makeLocal<VarInt>(loc, "OPT_CAPATH", "", CURLOPT_CAPATH);
    vm.makeLocal<VarInt>(loc, "OPT_CRLFILE", "", CURLOPT_CRLFILE);
    vm.makeLocal<VarInt>(loc, "OPT_CERTINFO", "", CURLOPT_CERTINFO);
    vm.makeLocal<VarInt>(loc, "OPT_PINNEDPUBLICKEY", "", CURLOPT_PINNEDPUBLICKEY);
    vm.makeLocal<VarInt>(loc, "OPT_SSL_CIPHER_LIST", "", CURLOPT_SSL_CIPHER_LIST);
#if CURL_AT_LEAST_VERSION(7, 61, 0)
    vm.makeLocal<VarInt>(loc, "OPT_TLS13_CIPHERS", "", CURLOPT_TLS13_CIPHERS);
    vm.makeLocal<VarInt>(loc, "OPT_PROXY_TLS13_CIPHERS", "", CURLOPT_PROXY_TLS13_CIPHERS);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_SSL_SESSIONID_CACHE", "", CURLOPT_SSL_SESSIONID_CACHE);
    vm.makeLocal<VarInt>(loc, "OPT_SSL_OPTIONS", "", CURLOPT_SSL_OPTIONS);
    vm.makeLocal<VarInt>(loc, "OPT_KRBLEVEL", "", CURLOPT_KRBLEVEL);
    vm.makeLocal<VarInt>(loc, "OPT_GSSAPI_DELEGATION", "", CURLOPT_GSSAPI_DELEGATION);

    // SSH OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_SSH_AUTH_TYPES", "", CURLOPT_SSH_AUTH_TYPES);
#if CURL_AT_LEAST_VERSION(7, 56, 0)
    vm.makeLocal<VarInt>(loc, "OPT_SSH_COMPRESSION", "", CURLOPT_SSH_COMPRESSION);
#endif
    vm.makeLocal<VarInt>(loc, "OPT_SSH_HOST_PUBLIC_KEY_MD5", "", CURLOPT_SSH_HOST_PUBLIC_KEY_MD5);
    vm.makeLocal<VarInt>(loc, "OPT_SSH_PUBLIC_KEYFILE", "", CURLOPT_SSH_PUBLIC_KEYFILE);
    vm.makeLocal<VarInt>(loc, "OPT_SSH_PRIVATE_KEYFILE", "", CURLOPT_SSH_PRIVATE_KEYFILE);
    vm.makeLocal<VarInt>(loc, "OPT_SSH_KNOWNHOSTS", "", CURLOPT_SSH_KNOWNHOSTS);
    vm.makeLocal<VarInt>(loc, "OPT_SSH_KEYFUNCTION", "", CURLOPT_SSH_KEYFUNCTION);
    vm.makeLocal<VarInt>(loc, "OPT_SSH_KEYDATA", "", CURLOPT_SSH_KEYDATA);

    // OTHER OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_PRIVATE", "", CURLOPT_PRIVATE);
    vm.makeLocal<VarInt>(loc, "OPT_SHARE", "", CURLOPT_SHARE);
    vm.makeLocal<VarInt>(loc, "OPT_NEW_FILE_PERMS", "", CURLOPT_NEW_FILE_PERMS);
    vm.makeLocal<VarInt>(loc, "OPT_NEW_DIRECTORY_PERMS", "", CURLOPT_NEW_DIRECTORY_PERMS);

    // TELNET OPTIONS
    vm.makeLocal<VarInt>(loc, "OPT_TELNETOPTIONS", "", CURLOPT_TELNETOPTIONS);

    // CURLINFO

    vm.makeLocal<VarInt>(loc, "INFO_EFFECTIVE_URL", "", CURLINFO_EFFECTIVE_URL);
    vm.makeLocal<VarInt>(loc, "INFO_RESPONSE_CODE", "", CURLINFO_RESPONSE_CODE);
    vm.makeLocal<VarInt>(loc, "INFO_TOTAL_TIME", "", CURLINFO_TOTAL_TIME);
    vm.makeLocal<VarInt>(loc, "INFO_NAMELOOKUP_TIME", "", CURLINFO_NAMELOOKUP_TIME);
    vm.makeLocal<VarInt>(loc, "INFO_CONNECT_TIME", "", CURLINFO_CONNECT_TIME);
    vm.makeLocal<VarInt>(loc, "INFO_PRETRANSFER_TIME", "", CURLINFO_PRETRANSFER_TIME);
    vm.makeLocal<VarInt>(loc, "INFO_SIZE_UPLOAD_T", "", CURLINFO_SIZE_UPLOAD_T);
    vm.makeLocal<VarInt>(loc, "INFO_SIZE_DOWNLOAD_T", "", CURLINFO_SIZE_DOWNLOAD_T);
    vm.makeLocal<VarInt>(loc, "INFO_SPEED_DOWNLOAD_T", "", CURLINFO_SPEED_DOWNLOAD_T);
    vm.makeLocal<VarInt>(loc, "INFO_SPEED_UPLOAD_T", "", CURLINFO_SPEED_UPLOAD_T);
    vm.makeLocal<VarInt>(loc, "INFO_HEADER_SIZE", "", CURLINFO_HEADER_SIZE);
    vm.makeLocal<VarInt>(loc, "INFO_REQUEST_SIZE", "", CURLINFO_REQUEST_SIZE);
    vm.makeLocal<VarInt>(loc, "INFO_SSL_VERIFYRESULT", "", CURLINFO_SSL_VERIFYRESULT);
    vm.makeLocal<VarInt>(loc, "INFO_FILETIME", "", CURLINFO_FILETIME);
    vm.makeLocal<VarInt>(loc, "INFO_FILETIME_T", "", CURLINFO_FILETIME_T);
    vm.makeLocal<VarInt>(loc, "INFO_CONTENT_LENGTH_DOWNLOAD_T", "",
                         CURLINFO_CONTENT_LENGTH_DOWNLOAD_T);
    vm.makeLocal<VarInt>(loc, "INFO_CONTENT_LENGTH_UPLOAD_T", "", CURLINFO_CONTENT_LENGTH_UPLOAD_T);
    vm.makeLocal<VarInt>(loc, "INFO_STARTTRANSFER_TIME", "", CURLINFO_STARTTRANSFER_TIME);
    vm.makeLocal<VarInt>(loc, "INFO_CONTENT_TYPE", "", CURLINFO_CONTENT_TYPE);
    vm.makeLocal<VarInt>(loc, "INFO_REDIRECT_TIME", "", CURLINFO_REDIRECT_TIME);
    vm.makeLocal<VarInt>(loc, "INFO_REDIRECT_COUNT", "", CURLINFO_REDIRECT_COUNT);
    vm.makeLocal<VarInt>(loc, "INFO_PRIVATE", "", CURLINFO_PRIVATE);
    vm.makeLocal<VarInt>(loc, "INFO_HTTP_CONNECTCODE", "", CURLINFO_HTTP_CONNECTCODE);
    vm.makeLocal<VarInt>(loc, "INFO_HTTPAUTH_AVAIL", "", CURLINFO_HTTPAUTH_AVAIL);
    vm.makeLocal<VarInt>(loc, "INFO_PROXYAUTH_AVAIL", "", CURLINFO_PROXYAUTH_AVAIL);
    vm.makeLocal<VarInt>(loc, "INFO_OS_ERRNO", "", CURLINFO_OS_ERRNO);
    vm.makeLocal<VarInt>(loc, "INFO_NUM_CONNECTS", "", CURLINFO_NUM_CONNECTS);
    vm.makeLocal<VarInt>(loc, "INFO_SSL_ENGINES", "", CURLINFO_SSL_ENGINES);
    vm.makeLocal<VarInt>(loc, "INFO_COOKIELIST", "", CURLINFO_COOKIELIST);
    vm.makeLocal<VarInt>(loc, "INFO_FTP_ENTRY_PATH", "", CURLINFO_FTP_ENTRY_PATH);
    vm.makeLocal<VarInt>(loc, "INFO_REDIRECT_URL", "", CURLINFO_REDIRECT_URL);
    vm.makeLocal<VarInt>(loc, "INFO_PRIMARY_IP", "", CURLINFO_PRIMARY_IP);
    vm.makeLocal<VarInt>(loc, "INFO_APPCONNECT_TIME", "", CURLINFO_APPCONNECT_TIME);
    vm.makeLocal<VarInt>(loc, "INFO_CERTINFO", "", CURLINFO_CERTINFO);
    vm.makeLocal<VarInt>(loc, "INFO_CONDITION_UNMET", "", CURLINFO_CONDITION_UNMET);
    vm.makeLocal<VarInt>(loc, "INFO_RTSP_SESSION_ID", "", CURLINFO_RTSP_SESSION_ID);
    vm.makeLocal<VarInt>(loc, "INFO_RTSP_CLIENT_CSEQ", "", CURLINFO_RTSP_CLIENT_CSEQ);
    vm.makeLocal<VarInt>(loc, "INFO_RTSP_SERVER_CSEQ", "", CURLINFO_RTSP_SERVER_CSEQ);
    vm.makeLocal<VarInt>(loc, "INFO_RTSP_CSEQ_RECV", "", CURLINFO_RTSP_CSEQ_RECV);
    vm.makeLocal<VarInt>(loc, "INFO_PRIMARY_PORT", "", CURLINFO_PRIMARY_PORT);
    vm.makeLocal<VarInt>(loc, "INFO_LOCAL_IP", "", CURLINFO_LOCAL_IP);
    vm.makeLocal<VarInt>(loc, "INFO_LOCAL_PORT", "", CURLINFO_LOCAL_PORT);
    vm.makeLocal<VarInt>(loc, "INFO_ACTIVESOCKET", "", CURLINFO_ACTIVESOCKET);
    vm.makeLocal<VarInt>(loc, "INFO_TLS_SSL_PTR", "", CURLINFO_TLS_SSL_PTR);
    vm.makeLocal<VarInt>(loc, "INFO_HTTP_VERSION", "", CURLINFO_HTTP_VERSION);
    vm.makeLocal<VarInt>(loc, "INFO_PROXY_SSL_VERIFYRESULT", "", CURLINFO_PROXY_SSL_VERIFYRESULT);
    vm.makeLocal<VarInt>(loc, "INFO_SCHEME", "", CURLINFO_SCHEME);
    vm.makeLocal<VarInt>(loc, "INFO_TOTAL_TIME_T", "", CURLINFO_TOTAL_TIME_T);
    vm.makeLocal<VarInt>(loc, "INFO_NAMELOOKUP_TIME_T", "", CURLINFO_NAMELOOKUP_TIME_T);
    vm.makeLocal<VarInt>(loc, "INFO_CONNECT_TIME_T", "", CURLINFO_CONNECT_TIME_T);
    vm.makeLocal<VarInt>(loc, "INFO_PRETRANSFER_TIME_T", "", CURLINFO_PRETRANSFER_TIME_T);
    vm.makeLocal<VarInt>(loc, "INFO_STARTTRANSFER_TIME_T", "", CURLINFO_STARTTRANSFER_TIME_T);
    vm.makeLocal<VarInt>(loc, "INFO_REDIRECT_TIME_T", "", CURLINFO_REDIRECT_TIME_T);
    vm.makeLocal<VarInt>(loc, "INFO_APPCONNECT_TIME_T", "", CURLINFO_APPCONNECT_TIME_T);
    vm.makeLocal<VarInt>(loc, "INFO_RETRY_AFTER", "", CURLINFO_RETRY_AFTER);
    vm.makeLocal<VarInt>(loc, "INFO_EFFECTIVE_METHOD", "", CURLINFO_EFFECTIVE_METHOD);
    vm.makeLocal<VarInt>(loc, "INFO_PROXY_ERROR", "", CURLINFO_PROXY_ERROR);
    vm.makeLocal<VarInt>(loc, "INFO_REFERER", "", CURLINFO_REFERER);
    vm.makeLocal<VarInt>(loc, "INFO_CAINFO", "", CURLINFO_CAINFO);
    vm.makeLocal<VarInt>(loc, "INFO_CAPATH", "", CURLINFO_CAPATH);
    vm.makeLocal<VarInt>(loc, "INFO_XFER_ID", "", CURLINFO_XFER_ID);
    vm.makeLocal<VarInt>(loc, "INFO_CONN_ID", "", CURLINFO_CONN_ID);
    vm.makeLocal<VarInt>(loc, "INFO_QUEUE_TIME_T", "", CURLINFO_QUEUE_TIME_T);
    vm.makeLocal<VarInt>(loc, "INFO_USED_PROXY", "", CURLINFO_USED_PROXY);
    vm.makeLocal<VarInt>(loc, "INFO_POSTTRANSFER_TIME_T", "", CURLINFO_POSTTRANSFER_TIME_T);
    vm.makeLocal<VarInt>(loc, "INFO_EARLYDATA_SENT_T", "", CURLINFO_EARLYDATA_SENT_T);
    vm.makeLocal<VarInt>(loc, "INFO_HTTPAUTH_USED", "", CURLINFO_HTTPAUTH_USED);
    vm.makeLocal<VarInt>(loc, "INFO_PROXYAUTH_USED", "", CURLINFO_PROXYAUTH_USED);
    vm.makeLocal<VarInt>(loc, "INFO_LASTONE", "", CURLINFO_LASTONE);

    // Auth

    vm.makeLocal<VarInt>(loc, "AUTH_BASIC", "", CURLAUTH_BASIC);
    vm.makeLocal<VarInt>(loc, "AUTH_DIGEST", "", CURLAUTH_DIGEST);
    vm.makeLocal<VarInt>(loc, "AUTH_DIGEST_IE", "", CURLAUTH_DIGEST_IE);
    vm.makeLocal<VarInt>(loc, "AUTH_BEARER", "", CURLAUTH_BEARER);
    vm.makeLocal<VarInt>(loc, "AUTH_NEGOTIATE", "", CURLAUTH_NEGOTIATE);
    vm.makeLocal<VarInt>(loc, "AUTH_NTLM", "", CURLAUTH_NTLM);
    vm.makeLocal<VarInt>(loc, "AUTH_NTLM_WB", "", CURLAUTH_NTLM_WB);
    vm.makeLocal<VarInt>(loc, "AUTH_ANY", "", CURLAUTH_ANY);
    vm.makeLocal<VarInt>(loc, "AUTH_ANYSAFE", "", CURLAUTH_ANYSAFE);
    vm.makeLocal<VarInt>(loc, "AUTH_ONLY", "", CURLAUTH_ONLY);
    vm.makeLocal<VarInt>(loc, "AUTH_AWS_SIGV4", "", CURLAUTH_AWS_SIGV4);

    // Multi

    vm.makeLocal<VarInt>(loc, "M_OK", "", CURLM_OK);
    vm.makeLocal<VarInt>(loc, "M_BAD_HANDLE", "", CURLM_BAD_HANDLE);
    vm.makeLocal<VarInt>(loc, "M_BAD_EASY_HANDLE", "", CURLM_BAD_EASY_HANDLE);
    vm.makeLocal<VarInt>(loc, "M_OUT_OF_MEMORY", "", CURLM_OUT_OF_MEMORY);
    vm.makeLocal<VarInt>(loc, "M_INTERNAL_ERROR", "", CURLM_INTERNAL_ERROR);
    vm.makeLocal<VarInt>(loc, "M_BAD_SOCKET", "", CURLM_BAD_SOCKET);
    vm.makeLocal<VarInt>(loc, "M_UNKNOWN_OPTION", "", CURLM_UNKNOWN_OPTION);
    vm.makeLocal<VarInt>(loc, "M_ADDED_ALREADY", "", CURLM_ADDED_ALREADY);
    vm.makeLocal<VarInt>(loc, "M_RECURSIVE_API_CALL", "", CURLM_RECURSIVE_API_CALL);
    vm.makeLocal<VarInt>(loc, "M_WAKEUP_FAILURE", "", CURLM_WAKEUP_FAILURE);
    vm.makeLocal<VarInt>(loc, "M_BAD_FUNCTION_ARGUMENT", "", CURLM_BAD_FUNCTION_ARGUMENT);
    vm.makeLocal<VarInt>(loc, "M_ABORTED_BY_CALLBACK", "", CURLM_ABORTED_BY_CALLBACK);
    vm.makeLocal<VarInt>(loc, "M_UNRECOVERABLE_POLL", "", CURLM_UNRECOVERABLE_POLL);

    // Msg

    vm.makeLocal<VarInt>(loc, "MSG_DONE", "", CURLMSG_DONE);
}

} // namespace fer