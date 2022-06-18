/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"

#if defined(_DEBUG) && defined(MEMDBGOK)
#include <array>
#include <ICoreGameInit.h>

static __declspec(thread) const char* g_newFile = "";
static __declspec(thread) size_t g_newLine;
static __declspec(thread) bool g_inAlloc = false;
bool g_inAllocHook = false;

/*bool __declspec(dllexport) CoreSetMemDebugInfo(const char* file, size_t line)
{
	g_newFile = file;
	g_newLine = line;

	return true;
}*/

static LONG g_globalTrackedMemory;

static CRITICAL_SECTION g_memCritSec;
static _Guarded_by_(g_memCritSec) std::map<long, size_t>* g_memorySizes;
static _Guarded_by_(g_memCritSec) std::map<long, uint32_t>* g_memoryTimes;
static _Guarded_by_(g_memCritSec) std::map<long, std::array<uintptr_t, 128>> * g_memoryStacks;
static _Guarded_by_(g_memCritSec) std::set<long>* g_newAllocations;

void StoreAlloc(void* ptr, size_t size, long reqID, const char* newFile, const char* newFunc, size_t newLine);
void RemoveAlloc(void* ptr, size_t size, long reqID);

#define nNoMansLandSize 4

typedef struct _CrtMemBlockHeader
{
        struct _CrtMemBlockHeader * pBlockHeaderNext;
        struct _CrtMemBlockHeader * pBlockHeaderPrev;
        char *                      szFileName;
        int                         nLine;
#ifdef _WIN64
        /* These items are reversed on Win64 to eliminate gaps in the struct
         * and ensure that sizeof(struct)%16 == 0, so 16-byte alignment is
         * maintained in the debug heap.
         */
        int                         nBlockUse;
        size_t                      nDataSize;
#else  /* _WIN64 */
        size_t                      nDataSize;
        int                         nBlockUse;
#endif  /* _WIN64 */
        long                        lRequest;
        unsigned char               gap[nNoMansLandSize];
        /* followed by:
         *  unsigned char           data[nDataSize];
         *  unsigned char           anotherGap[nNoMansLandSize];
         */
} _CrtMemBlockHeader;

#define pHdr(pbData) (((_CrtMemBlockHeader *)pbData)-1)

static int  CrtAllocHook(int      nAllocType,
						 void   * pvData,
						 size_t   nSize,
						 int      nBlockUse,
						 long     lRequest,
						 const unsigned char * szFileName,
						 int      nLine)
{
	if (nBlockUse == _CRT_BLOCK)
	{
		return TRUE;
	}

	if (!g_inAllocHook)
	{
		g_inAllocHook = true;

		if (nAllocType == _HOOK_ALLOC)
		{
			StoreAlloc(pvData, nSize, lRequest, (g_newFile[0]) ? g_newFile : (const char*)szFileName, "", (g_newFile[0]) ? g_newLine : nLine);
		}
		else if (nAllocType == _HOOK_FREE)
		{
			if (_CrtIsValidHeapPointer(pvData))
			{
				_CrtMemBlockHeader* pHead = pHdr(pvData);
				lRequest = pHead->lRequest;
			}

			RemoveAlloc(pvData, nSize, lRequest);
		}

		// clear the new caller
		g_newFile = "";

		g_inAllocHook = false;
	}

	return TRUE;
}

static bool canTrace = false;

static InitFunction initFunction([] ()
{
	_CrtSetAllocHook(CrtAllocHook);

	std::thread([]()
	{
		while (true)
		{
			Sleep(0);

			if (GetAsyncKeyState(VK_F11) & 0x8000)
			{
				canTrace = true;
				return;
			}
		}
	}).detach();
}, -100);

static uint32_t g_lastTrackTime;

static void safe_memcpy(void* dst, const void* src, size_t size)
{
	__try
	{
		memcpy(dst, src, size);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{

	}
}

void StoreAlloc(void* ptr, size_t size, long reqID, const char* newFile, const char* newFunc, size_t newLine)
{
	if (!g_memCritSec.DebugInfo)
	{
		g_memorySizes = new std::map<long, size_t>();
		g_memoryTimes = new std::map<long, uint32_t>();
		g_memoryStacks = new std::map<long, std::array<uintptr_t, 128>>();
		g_newAllocations = new std::set<long>();

		InitializeCriticalSectionAndSpinCount(&g_memCritSec, 1000);
	}

	InterlockedAdd(&g_globalTrackedMemory, size);

	// manual OutputDebugString with Windows' sprintf functions to avoid CRT deadlocks on the setlocale lock
	char buffer[8192];
	wsprintfA(buffer, "%s(%d) %s : allocating %d bytes (%d - %d used)\n", newFile, newLine, newFunc, size, reqID, g_globalTrackedMemory);

	//OutputDebugStringA(buffer);

	std::array<uintptr_t, 128> stackArr;
	safe_memcpy(stackArr.data(), _AddressOfReturnAddress(), sizeof(stackArr));

	EnterCriticalSection(&g_memCritSec);
	(*g_memorySizes)[reqID] = size;
	(*g_memoryTimes)[reqID] = GetTickCount();
	(*g_memoryStacks)[reqID] = stackArr;
	g_newAllocations->insert(reqID);

	if ((GetTickCount() - g_lastTrackTime) > 2500)
	{
		if (canTrace)
		{
			auto time = GetTickCount();

			wsprintfA(buffer, "--- ALLOCATION HIT LIST (%d) ---\n", g_globalTrackedMemory);

			OutputDebugStringA(buffer);

			for (auto& alloc : *g_newAllocations)
			{
				auto timeDiff = time - (*g_memoryTimes)[alloc];

				if (timeDiff > 2000)
				{
					wsprintfA(buffer, "%d - %d (%dms)\n", alloc, (*g_memorySizes)[alloc], timeDiff);

					OutputDebugStringA(buffer);

					auto& stack = (*g_memoryStacks)[alloc];

					for (int i = 48; i < stackArr.size(); i += 8)
					{
						wsprintfA(buffer, "%p %p %p %p %p %p %p %p\n", stack[i], stack[i + 1], stack[i + 2], stack[i + 3], stack[i + 4], stack[i + 5], stack[i + 6], stack[i + 7]);
						OutputDebugStringA(buffer);
					}
				}
			}

			wsprintfA(buffer, "--- TOTAL: %d ---\n", g_newAllocations->size());

			OutputDebugStringA(buffer);
		}

		g_newAllocations->clear();

		(*g_memoryStacks).clear();

		g_lastTrackTime = GetTickCount();
	}

	LeaveCriticalSection(&g_memCritSec);
}

void RemoveAlloc(void* ptr, size_t size, long reqID)
{
	InterlockedAdd(&g_globalTrackedMemory, -(intptr_t)(*g_memorySizes)[reqID]);

	// manual OutputDebugString with Windows' sprintf functions to avoid CRT deadlocks on the setlocale lock
	char buffer[8192];
	wsprintfA(buffer, "%p/%d freed\n", ptr, reqID);

	//OutputDebugStringA(buffer);

	EnterCriticalSection(&g_memCritSec);
	g_memorySizes->erase(reqID);
	g_memoryTimes->erase(reqID);
	g_memoryStacks->erase(reqID);
	g_newAllocations->erase(reqID);
	LeaveCriticalSection(&g_memCritSec);
}
#endif
