#include <StdInc.h>
#include <Pool.h>

#include <ScriptEngine.h>

#include <DirectXMath.h>

#include <Hooking.h>
#include <CoreConsole.h>
#include <Resource.h>
#include <fxScripting.h>

#include <nutsnbolts.h>

#include <RageParser.h>

static hook::cdecl_stub<void* (const uint32_t& hash, void* typeAssert)> _getCameraMetadata([]()
{
	return hook::get_call(hook::get_pattern("C7 44 24 48 63 1C 9E D7 E8", 8));
});

static std::shared_ptr<ConVar<bool>> g_handbrakeCamConvar;

struct camFollowVehicleCameraMetadataHandBrakeSwingSettings
{
	void* vtbl;
	uint32_t HandBrakeInputEnvelopeRef;
	float SpringConstant;
	float MinLateralSkidSpeed;
	float MaxLateralSkidSpeed;
	float SwingSpeedAtMaxSkidSpeed;
};

static void* g_currentCamMetadata;
static camFollowVehicleCameraMetadataHandBrakeSwingSettings g_lastSettings;
static bool g_lastMetadataState;

static ptrdiff_t GetCamMetadataOffset()
{
	auto tcurts = rage::GetStructureDefinition("camFollowVehicleCameraMetadata");
	for (auto& member : tcurts->m_members)
	{
		if (member->m_definition->hash == HashRageString("HandBrakeSwingSettings"))
		{
			return member->m_definition->offset;
		}
	}

	return 0;
}

static auto GetRelativeMetadata(void* base)
{
	static ptrdiff_t offsetRef = GetCamMetadataOffset();

	return (camFollowVehicleCameraMetadataHandBrakeSwingSettings*)((char*)base + offsetRef);
}

static void OverrideMetadata(void* metadata, bool overridden)
{
	if (g_lastMetadataState != overridden)
	{
		auto camhb = GetRelativeMetadata(metadata);

		if (overridden)
		{
			g_lastSettings = *camhb;

			camhb->SpringConstant = 0.f;
			camhb->MaxLateralSkidSpeed = 0.f;
			camhb->MinLateralSkidSpeed = 0.f;
			camhb->SwingSpeedAtMaxSkidSpeed = 0.f;
		}
		else
		{
			*camhb = g_lastSettings;
		}

		g_lastMetadataState = overridden;
	}
}

static void* GetCameraMetadataWrap(const uint32_t& hash, void* typeAssert)
{
	auto metadata = _getCameraMetadata(hash, typeAssert);

	if (g_currentCamMetadata)
	{
		OverrideMetadata(g_currentCamMetadata, false);
	}

	g_currentCamMetadata = metadata;

	if (!g_handbrakeCamConvar->GetValue())
	{
		OverrideMetadata(g_currentCamMetadata, true);
	}

	return metadata;
}

static void UpdateCameraMetadataRef()
{
	if (g_currentCamMetadata)
	{
		auto curValue = !(g_handbrakeCamConvar->GetValue());

		if (curValue != g_lastMetadataState)
		{
			OverrideMetadata(g_currentCamMetadata, curValue);
		}
	}
}

using Matrix4x4 = DirectX::XMFLOAT4X4;

struct CameraData
{
	uint8_t m_pad[48];
	Matrix4x4 m_matrix1;
	char m_pad2[160];
	Matrix4x4 m_matrix2;
	char m_pad3[208];
	uint8_t m_currentMatrix : 1;
};

struct scrVector
{
	float x;
	int _pad;
	float y;
	int _pad2;
	float z;
	int _pad3;
};

struct camBaseObject
{
	virtual void DESTROY() = 0;
	virtual bool IsHashTypeOf(int hash) = 0;
	virtual int GetCamHash() = 0;
	virtual int GetHash2() = 0;   // Added in 2189, not unique, maybe a category
	virtual bool CanUpdate() = 0; // Checks realtime - lastUpdatedTime > metadata.TimeoutMS
	virtual void sub_1402D5D94() = 0;
	virtual void UpdateCameraState() = 0;
	virtual void sub_14026DF58() = 0;
	virtual void sub_1402E8C64() = 0;
	virtual void sub_14030443C() = 0;
	virtual bool ReturnMetadataBool() = 0;
};

typedef bool (*camCanUpdateFn)(camBaseObject* thisptr);
static camCanUpdateFn origCamCinematicOnFootIdleContext_CanUpdate;

static std::atomic_bool g_DISABLE_IDLE_CAM = false;

static bool CamCinematicOnFootIdleContext_CanUpdate(camBaseObject* thisptr)
{
	if (!g_DISABLE_IDLE_CAM)
	{
		return origCamCinematicOnFootIdleContext_CanUpdate(thisptr);
	}
	return false;
}

static HookFunction hookFunction([]()
{
	hook::call(hook::get_pattern("48 8D 4D 20 44 89 75 20 E8 ? ? ? ? 48 85 C0", 8), GetCameraMetadataWrap);

	uintptr_t* camCinematicOnFootIdleContext_vtable = hook::get_address<uintptr_t*>(hook::get_pattern<unsigned char>("48 8D 05 ? ? ? ? 48 89 07 48 8B C7 F3 0F 10 ? ? ? ? 02 F3 0F 11 47 60", 3));

	int index = 3;
	// 2189 Added another vfunc between
	if (xbr::IsGameBuildOrGreater<2189>())
	{
		index++;
	}

	origCamCinematicOnFootIdleContext_CanUpdate = (camCanUpdateFn)camCinematicOnFootIdleContext_vtable[index];
	hook::put(&camCinematicOnFootIdleContext_vtable[index], (uintptr_t)CamCinematicOnFootIdleContext_CanUpdate);
});

static InitFunction initFunction([]()
{
	fx::ScriptEngine::RegisterNativeHandler("GET_CAM_MATRIX", [](fx::ScriptContext& scriptContext)
	{
		auto camIndex = scriptContext.GetArgument<int>(0);

		auto camPool = rage::GetPoolBase(0xFE12CE88);
		auto cam = camPool->GetAtHandle<CameraData>(camIndex);

		if (cam != nullptr)
		{
			Matrix4x4* readMatrix;

			// get the right matrix
			if (cam->m_currentMatrix)
			{
				readMatrix = &cam->m_matrix2;
			}
			else
			{
				readMatrix = &cam->m_matrix1;
			}

			// write to output
			scrVector* rightVector = scriptContext.GetArgument<scrVector*>(1);
			scrVector* forwardVector = scriptContext.GetArgument<scrVector*>(2);
			scrVector* upVector = scriptContext.GetArgument<scrVector*>(3);
			scrVector* atVector = scriptContext.GetArgument<scrVector*>(4);

			auto copyVector = [](const float* in, scrVector* out)
			{
				out->x = in[0];
				out->y = in[1];
				out->z = in[2];
			};

			copyVector(readMatrix->m[0], rightVector);
			copyVector(readMatrix->m[1], forwardVector);
			copyVector(readMatrix->m[2], upVector);
			copyVector(readMatrix->m[3], atVector);
		}
	});

	fx::ScriptEngine::RegisterNativeHandler("DISABLE_IDLE_CAMERA", [](fx::ScriptContext& context)
	{
		bool value = context.GetArgument<bool>(0);
		g_DISABLE_IDLE_CAM = value;

		fx::OMPtr<IScriptRuntime> runtime;
		if (FX_SUCCEEDED(fx::GetCurrentScriptRuntime(&runtime)))
		{
			fx::Resource* resource = reinterpret_cast<fx::Resource*>(runtime->GetParentObject());

			resource->OnStop.Connect([]()
			{
				g_DISABLE_IDLE_CAM = false;
			});
		}
	});

	g_handbrakeCamConvar = std::make_shared<ConVar<bool>>("cam_enableHandbrakeCamera", ConVar_Archive, true);

	OnMainGameFrame.Connect([]()
	{
		UpdateCameraMetadataRef();
	});
});
