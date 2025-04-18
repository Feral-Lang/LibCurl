#pragma once

#include <curl/curl.h>
#include <VM/Interpreter.hpp>

namespace fer
{

//////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// VarCurl //////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

class VarCurl : public Var
{
	CURL *val;
	UniList<curl_mime *> mimelist; // list of mimes (for tracking memory)
	VarFn *progCB;
	VarFn *writeCB;
	// If this is not nullptr, it's guaranteed to have 5 elements which are reserved:
	// nullptr, dlTotal (float), dlDone (float), ulTotal (float), ulDone (float)
	VarVec *progCBArgs;
	// If this is not nullptr, it's guaranteed to have 2 elements which are reserved:
	// nullptr, dataToWrite (string)
	VarVec *writeCBArgs;
	size_t progIntervalTick;
	size_t progIntervalTickMax;

	void onCreate(MemoryManager &mem) override;
	void onDestroy(MemoryManager &mem) override;

public:
	VarCurl(ModuleLoc loc, CURL *val);
	~VarCurl();

	// _progCB can be nullptr, and args can have zero elements
	void setProgressCB(MemoryManager &mem, VarFn *_progCB, Span<Var *> args);
	// _writeCB can be nullptr, and args can have zero elements
	void setWriteCB(MemoryManager &mem, VarFn *_writeCB, Span<Var *> args);
	// data can be either VarMap or VarStr: if it's VarStr, the string is used as filename
	curl_mime *createMime(VirtualMachine &vm, ModuleLoc loc, Var *data);
	void clearMimeData();

	inline void setProgIntervalTickMax(size_t maxVal) { progIntervalTickMax = maxVal; }

	inline CURL *const getVal() { return val; }
	inline VarFn *getProgressCB() { return progCB; }
	inline VarFn *getWriteCB() { return writeCB; }
	inline VarVec *getProgressCBArgs() { return progCBArgs; }
	inline VarVec *getWriteCBArgs() { return writeCBArgs; }
	inline size_t &getProgIntervalTick() { return progIntervalTick; }
	inline size_t getProgIntervalTickMax() { return progIntervalTickMax; }
};

struct CurlCallbackData
{
	ModuleLoc loc;
	VirtualMachine &vm;
	VarCurl *curl;
	CurlCallbackData(ModuleLoc loc, VirtualMachine &vm, VarCurl *curl);
};

} // namespace fer