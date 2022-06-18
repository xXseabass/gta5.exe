/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "Hooking.h"

namespace hook
{
#ifndef _M_AMD64
	void inject_hook::inject()
	{
		inject_hook_frontend fe(this);
		m_assembly = std::make_shared<FunctionAssembly>(fe);

		put<uint8_t>(m_address, 0xE9);
		put<int>(m_address + 1, (uintptr_t)m_assembly->GetCode() - (uintptr_t)get_adjusted(m_address) - 5);
	}

	void inject_hook::injectCall()
	{
		inject_hook_frontend fe(this);
		m_assembly = std::make_shared<FunctionAssembly>(fe);

		put<uint8_t>(m_address, 0xE8);
		put<int>(m_address + 1, (uintptr_t)m_assembly->GetCode() - (uintptr_t)get_adjusted(m_address) - 5);
	}
#else
	void* AllocateFunctionStub(void* ptr, int type)
	{
#if defined(GTA_FIVE) || defined(IS_RDR3)
		typedef void*(*AllocateType)(void*, int);
		static AllocateType func;

		if (func == nullptr)
		{
			HMODULE coreRuntime = GetModuleHandleW(L"CoreRT.dll");
			func = (AllocateType)GetProcAddress(coreRuntime, "AllocateFunctionStubImpl");
		}

		return func(ptr, type);
#else
		return ptr;
#endif
	}

	void* AllocateStubMemory(size_t size)
	{
#if defined(GTA_FIVE) || defined(IS_RDR3)
		static decltype(&AllocateStubMemory) func;

		if (func == nullptr)
		{
			HMODULE coreRuntime = GetModuleHandleW(L"CoreRT.dll");
			func = (decltype(func))GetProcAddress(coreRuntime, "AllocateStubMemoryImpl");
		}

		return func(size);
#else
		return nullptr;
#endif
	}
#endif

	ptrdiff_t baseAddressDifference;
}

#ifndef IS_FXSERVER
static InitFunction initHookingFunction([]()
{
	hook::set_base();
}, INT32_MIN);
#endif
