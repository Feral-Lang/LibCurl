#pragma once

#include <curl/curl.h>
#include <VM/VM.hpp>

namespace fer
{

class VarCurlMulti;

//////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////// VarCurlEasy ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

class VarCurlEasy : public Var
{
    struct CallbackData
    {
        VirtualMachine *vm;
        ModuleLoc loc;
    } cbdata;

    CURL *val;

    UniList<curl_mime *> mimelist; // list of mimes (for tracking memory)
    UniList<curl_slist *> sllist;  // list of list of strings (for tracking memory)

    // If this is not nullptr, it's guaranteed to have 5 elements which are reserved:
    // nullptr, dlTotal (float), dlDone (float), ulTotal (float), ulDone (float)
    VarClosure *progCB;
    // If this is not nullptr, it's guaranteed to have 2 elements which are reserved:
    // nullptr, dataToWrite (bytebuffer)
    VarClosure *writeCB;
    size_t progIntervalTick;
    size_t progIntervalTickMax;
    bool done;

    void onCreate(VirtualMachine &vm) override;
    void onDestroy(VirtualMachine &vm) override;

public:
    VarCurlEasy(ModuleLoc loc, CURL *val);
    ~VarCurlEasy();

    void setCbData(VirtualMachine *vm, ModuleLoc loc);
    // _progCB can be nullptr
    void setProgressCB(VirtualMachine &vm, VarClosure *_progCB);
    // _writeCB can be nullptr
    void setWriteCB(VirtualMachine &vm, VarClosure *_writeCB);
    // data can be either VarMap or VarStr: if it's VarStr, the string is used as filename
    bool createMime(VirtualMachine &vm, ModuleLoc loc, Var *data, curl_mime *&mime);
    void clearMimeData();
    bool createSList(VirtualMachine &vm, ModuleLoc loc, Var *data, curl_slist *&lst);
    void clearSList();

    inline void setProgIntervalTickMax(size_t maxVal) { progIntervalTickMax = maxVal; }
    inline void setDone(bool isDone) { done = isDone; }

    inline VirtualMachine *getCbVM() { return cbdata.vm; }
    inline ModuleLoc getCbLoc() { return cbdata.loc; }
    inline CURL *const getVal() { return val; }
    inline VarClosure *getProgressCB() { return progCB; }
    inline VarClosure *getWriteCB() { return writeCB; }
    inline size_t &getProgIntervalTick() { return progIntervalTick; }
    inline size_t getProgIntervalTickMax() { return progIntervalTickMax; }
    inline bool isDone() { return done; }
};

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// VarCurlMulti ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

class VarCurlMulti : public Var
{
    RecursiveMutex mtx;
    CURLM *val;
    size_t pollMs;
    VarVec *easyHandles;

    void onCreate(VirtualMachine &vm) override;
    void onDestroy(VirtualMachine &vm) override;

public:
    VarCurlMulti(ModuleLoc loc, CURLM *val, size_t pollMs);
    ~VarCurlMulti();

    void addEasy(VirtualMachine &vm, VarCurlEasy *easy, bool iref);
    VarCurlEasy *remEasy(VirtualMachine &vm, ModuleLoc loc, CURL *easy, bool dref);

    inline RecursiveMutex &getMutex() { return mtx; }
    inline CURLM *const getVal() { return val; }
    inline size_t getPollMs() { return pollMs; }
    inline VarVec *getEasyHandles() { return easyHandles; }
};

} // namespace fer