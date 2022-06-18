/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include <Resource.h>
#include <Error.h>

#include <ResourceGameLifetimeEvents.h>

// #TODOLIBERTY
#ifndef GTA_NY
#include <ScriptHandlerMgr.h>
#include <scrThread.h>
#include <scrEngine.h>

#include <ResourceMetaDataComponent.h>
#include <ManifestVersion.h>

#include <ICoreGameInit.h>

#include <Pool.h>

#include <sysAllocator.h>

#include <stack>

void CoreRT_SetHardening(bool hardened)
{
#ifdef GTA_FIVE
	using TCoreFunc = decltype(&CoreRT_SetHardening);

	static TCoreFunc func;

	if (!func)
	{
		auto hCore = GetModuleHandleW(L"CoreRT.dll");

		if (hCore)
		{
			func = (TCoreFunc)GetProcAddress(hCore, "CoreRT_SetHardening");
		}
	}

	return (func) ? func(hardened) : 0;
#endif
}

struct DummyThread : public GtaThread
{
	DummyThread(fx::Resource* resource)
	{
		rage::scrThreadContext* context = GetContext();
		context->ScriptHash = HashString(resource->GetName().c_str());
		context->ThreadId = HashString(resource->GetName().c_str());

		SetScriptName(resource->GetName().c_str());
	}

	virtual void DoRun() override
	{

	}

	// zero-initialize the structure when new'd (to fix, for instance, 'can remove blips set by other scripts' defaulting to 0xCD)
	void* operator new(size_t size)
	{
		void* data = malloc(size);
		memset(data, 0, size);

		return data;
	}

	void operator delete(void* ptr)
	{
		free(ptr);
	}
};

static std::stack<rage::scrThread*> g_lastThreads;

#if defined(MISCLEAN_HAS_SCRIPT_PROCESS_TICK)
static std::stack<UpdatingScriptThreadsScope> g_scopes;
#endif

struct MissionCleanupData
{
	DummyThread* dummyThread;

	int behaviorVersion;

	bool cleanedUp;

	MissionCleanupData()
		: dummyThread(nullptr), cleanedUp(false)
	{

	}
};

GtaThread* g_resourceThread;

static void DeleteDummyThread(DummyThread** dummyThread)
{
	__try
	{
		if (*dummyThread)
		{
			OnDeleteResourceThread(*dummyThread);
		}

		delete *dummyThread;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{

	}

	*dummyThread = nullptr;
}

static struct : GtaThread
{
	virtual void DoRun() override
	{
	}
} g_globalLeftoverThread;

static constexpr ManifestVersion mfVer1 = "05cfa83c-a124-4cfa-a768-c24a5811d8f9";

static InitFunction initFunction([] ()
{
	fx::Resource::OnInitializeInstance.Connect([] (fx::Resource* resource)
	{
		// continue init
		auto data = std::make_shared<MissionCleanupData>();

		resource->OnStart.Connect([=] ()
		{
			data->dummyThread = nullptr;

			data->cleanedUp = false;

			auto metaData = resource->GetComponent<fx::ResourceMetaDataComponent>();

			data->behaviorVersion = 0;

			auto result = metaData->IsManifestVersionBetween(mfVer1.guid, guid_t{ 0 });

			if (result && *result && !Instance<ICoreGameInit>::Get()->OneSyncEnabled)
			{
				data->behaviorVersion = 1;
			}
		}, -10000);

		resource->OnActivate.Connect([=] ()
		{
			CoreRT_SetHardening(true);

			if (!Instance<ICoreGameInit>::Get()->GetGameLoaded())
			{
				return;
			}

			if (data->cleanedUp)
			{
				g_lastThreads.push(rage::scrEngine::GetActiveThread());

				rage::scrEngine::SetActiveThread(&g_globalLeftoverThread);
				return;
			}

			// ensure that we can call into game code here
			// #FIXME: should we not always be on the main thread?!
			rage::sysMemAllocator::UpdateAllocatorValue();

			bool setScriptNow = false;

			// create the script handler if needed
			if (!data->dummyThread)
			{
				data->dummyThread = new DummyThread(resource);

				OnCreateResourceThread(data->dummyThread, resource->GetName());
				CGameScriptHandlerMgr::GetInstance()->AttachScript(data->dummyThread);

				setScriptNow = true;
			}

			// set the current script handler
			GtaThread* gtaThread = data->dummyThread;

			g_lastThreads.push(rage::scrEngine::GetActiveThread());

#if defined(MISCLEAN_HAS_SCRIPT_PROCESS_TICK)
			g_scopes.emplace(true);
#endif

			rage::scrEngine::SetActiveThread(gtaThread);

			if (setScriptNow)
			{
				// make this a network script
				NativeInvoke::Invoke<0x1CA59E306ECB80A5, int>(32, false, -1);

				if (data->behaviorVersion >= 1)
				{
					// get script status; this sets a flag in the CGameScriptHandlerNetComponent
					NativeInvoke::Invoke<0x57D158647A6BFABF, int>();
				}
			}
		}, -10000);

		resource->OnDeactivate.Connect([=] ()
		{
			CoreRT_SetHardening(false);

			if (!Instance<ICoreGameInit>::Get()->GetGameLoaded())
			{
				return;
			}

			// only run if we have an active thread
			if (!rage::scrEngine::GetActiveThread())
			{
				return;
			}

#if defined(MISCLEAN_HAS_SCRIPT_PROCESS_TICK)
			if (!g_scopes.empty())
			{
				g_scopes.pop();
			}
#endif

			rage::scrThread* lastThread = nullptr;

			if (!g_lastThreads.empty())
			{
				lastThread = g_lastThreads.top();
				g_lastThreads.pop();
			}

			// restore the last thread
			rage::scrEngine::SetActiveThread(lastThread);
		}, 10000);

		auto cleanupResource = [=]()
		{
			if (!Instance<ICoreGameInit>::Get()->GetGameLoaded() || Instance<ICoreGameInit>::Get()->HasVariable("gameKilled"))
			{
				return;
			}

			if (data->cleanedUp)
			{
				return;
			}

			if (data->dummyThread && data->dummyThread->GetScriptHandler())
			{
				data->dummyThread->GetScriptHandler()->CleanupObjectList();
				CGameScriptHandlerMgr::GetInstance()->DetachScript(data->dummyThread);
			}

			// having the function content inlined causes a compiler ICE - so we do it separately
			DeleteDummyThread(&data->dummyThread);

			data->cleanedUp = true;
		};

		resource->GetComponent<fx::ResourceGameLifetimeEvents>()->OnBeforeGameShutdown.Connect([=]()
		{
			AddCrashometry("game_shutdown", "true");
			cleanupResource();
		});

		resource->GetComponent<fx::ResourceGameLifetimeEvents>()->OnGameDisconnect.Connect([=]()
		{
			AddCrashometry("game_disconnect", "true");
			cleanupResource();
		});

		resource->OnStop.Connect([=] ()
		{
			cleanupResource();
		}, 10000);
	}, -50);
});
#else
#include <ICoreGameInit.h>
#include <ResourceManager.h>

#include <MissionCleanup.h>
#include <stack>

#include <scrEngine.h>

GtaThread* g_resourceThread;
static std::stack<rage::scrThread*> g_lastThreads;

class ResourceMissionCleanupComponentNY : public fwRefCountable, public fx::IAttached<fx::Resource>
{
public:
	static void InitClass();

	virtual void AttachToObject(fx::Resource* resource) override
	{
		resource->OnActivate.Connect([this]()
		{
			if (!Instance<ICoreGameInit>::Get()->GetGameLoaded())
			{
				return;
			}

			g_lastThreads.push(rage::scrEngine::GetActiveThread());
			rage::scrEngine::SetActiveThread(g_resourceThread);

			// lazy-initialize so we only run when the game has loaded
			if (!m_cleanup)
			{
				m_cleanup = std::make_shared<CMissionCleanup>();
				m_cleanup->Initialize();
			}

			ms_activationStack.push(this);
		}, INT32_MIN);

		resource->OnDeactivate.Connect([this]()
		{
			if (!Instance<ICoreGameInit>::Get()->GetGameLoaded())
			{
				return;
			}

			// #TODO: do we need a DCHECK?
			assert(this == ms_activationStack.top().GetRef());
			ms_activationStack.pop();

			rage::scrThread* lastThread = nullptr;

			if (!g_lastThreads.empty())
			{
				lastThread = g_lastThreads.top();
				g_lastThreads.pop();
			}

			// restore the last thread
			rage::scrEngine::SetActiveThread(lastThread);
		}, INT32_MAX);

		auto cleanupResource = [this]()
		{
			if (m_cleanup)
			{
				m_cleanup->CleanUp(g_resourceThread);
			}
		};

		resource->GetComponent<fx::ResourceGameLifetimeEvents>()->OnBeforeGameShutdown.Connect([cleanupResource]()
		{
			AddCrashometry("game_shutdown", "true");
			cleanupResource();
		});

		resource->GetComponent<fx::ResourceGameLifetimeEvents>()->OnGameDisconnect.Connect([cleanupResource]()
		{
			AddCrashometry("game_disconnect", "true");
			cleanupResource();
		});

		resource->OnStop.Connect([cleanupResource]()
		{
			cleanupResource();
		},
		INT32_MAX);

		m_resource = resource;
	}

private:
	static std::stack<fwRefContainer<ResourceMissionCleanupComponentNY>> ms_activationStack;

	fx::Resource* m_resource;
	std::shared_ptr<CMissionCleanup> m_cleanup;
};

std::stack<fwRefContainer<ResourceMissionCleanupComponentNY>> ResourceMissionCleanupComponentNY::ms_activationStack;

DECLARE_INSTANCE_TYPE(ResourceMissionCleanupComponentNY);

void ResourceMissionCleanupComponentNY::InitClass()
{
	fx::Resource::OnInitializeInstance.Connect([](fx::Resource* resource)
	{
		resource->SetComponent(new ResourceMissionCleanupComponentNY);
	});

	CMissionCleanup::OnQueryMissionCleanup.Connect([](CMissionCleanup*& instance)
	{
		if (!ms_activationStack.empty())
		{
			instance = ms_activationStack.top()->m_cleanup.get();
		}
	});

	CMissionCleanup::OnCheckCollision.Connect([]()
	{
		Instance<fx::ResourceManager>::Get()->ForAllResources([](const fwRefContainer<fx::Resource>& resource)
		{
			auto selfComponent = resource->GetComponent<ResourceMissionCleanupComponentNY>();

			if (selfComponent->m_cleanup)
			{
				selfComponent->m_cleanup->CheckIfCollisionHasLoadedForMissionObjects();
			}
		});
	});
}

static InitFunction initFunction([]()
{
	ResourceMissionCleanupComponentNY::InitClass();
});
#endif

static InitFunction initFunctionRglt([]()
{
	fx::Resource::OnInitializeInstance.Connect([](fx::Resource* resource)
	{
		// TODO: factor this out elsewhere
		resource->SetComponent(new fx::ResourceGameLifetimeEvents());
	},
	INT32_MIN);
});
