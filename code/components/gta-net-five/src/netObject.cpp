#include <StdInc.h>

#include <Hooking.h>
#include <netObject.h>
#include <netSyncTree.h>

#include <atPool.h>
#include <Pool.h>

using TCreateCloneObjFn = rage::netObject*(*)(uint16_t objectId, uint8_t, int, int);
using TPoolPtr = atPoolBase**;

static TCreateCloneObjFn createCloneFuncs[(int)NetObjEntityType::Max];
static TPoolPtr validatePools[(int)NetObjEntityType::Max];

namespace rage
{
	netObject* CreateCloneObject(NetObjEntityType type, uint16_t objectId, uint8_t a2, int a3, int a4)
	{
		auto pool = *validatePools[(int)type];

		if (pool->GetCountDirect() >= pool->GetSize())
		{
			return nullptr;
		}

		if (type == NetObjEntityType::Ped || type == NetObjEntityType::Player)
		{
			auto entityPool = rage::GetPoolBase("Peds");

			if (entityPool->GetCountDirect() >= (entityPool->GetSize() - 4))
			{
				return nullptr;
			}
		}

		return createCloneFuncs[(int)type](objectId, a2, a3, a4);
	}
}

static HookFunction hookFunction([]()
{
	auto location = hook::get_pattern<char>("0F 8E 12 03 00 00 41 8A", 22);

	createCloneFuncs[(int)NetObjEntityType::Ped]				= (TCreateCloneObjFn)hook::get_call(location);
	createCloneFuncs[(int)NetObjEntityType::Object]				= (TCreateCloneObjFn)hook::get_call(location + 0x39);
	createCloneFuncs[(int)NetObjEntityType::Heli]				= (TCreateCloneObjFn)hook::get_call(location + 0x72);
	createCloneFuncs[(int)NetObjEntityType::Door]				= (TCreateCloneObjFn)hook::get_call(location + 0xAB);
	createCloneFuncs[(int)NetObjEntityType::Boat]				= (TCreateCloneObjFn)hook::get_call(location + 0xE4);
	createCloneFuncs[(int)NetObjEntityType::Bike]				= (TCreateCloneObjFn)hook::get_call(location + 0x11D);
	createCloneFuncs[(int)NetObjEntityType::Automobile]			= (TCreateCloneObjFn)hook::get_call(location + 0x156);
	createCloneFuncs[(int)NetObjEntityType::Pickup]				= (TCreateCloneObjFn)hook::get_call(location + 0x18F);
	createCloneFuncs[(int)NetObjEntityType::Train]				= (TCreateCloneObjFn)hook::get_call(location + 0x1EF);
	createCloneFuncs[(int)NetObjEntityType::Trailer]			= (TCreateCloneObjFn)hook::get_call(location + 0x228);
	createCloneFuncs[(int)NetObjEntityType::Player]				= (TCreateCloneObjFn)hook::get_call(location + 0x261);
	createCloneFuncs[(int)NetObjEntityType::Submarine]			= (TCreateCloneObjFn)hook::get_call(location + 0x296);
	createCloneFuncs[(int)NetObjEntityType::Plane]				= (TCreateCloneObjFn)hook::get_call(location + 0x2C8);
	createCloneFuncs[(int)NetObjEntityType::PickupPlacement]	= (TCreateCloneObjFn)hook::get_call(location + 0x2FA);

	validatePools[(int)NetObjEntityType::Ped]				= (TPoolPtr)hook::get_address<void*>(location - 42);
	validatePools[(int)NetObjEntityType::Object]			= (TPoolPtr)hook::get_address<void*>(location + 13);
	validatePools[(int)NetObjEntityType::Heli]				= (TPoolPtr)hook::get_address<void*>(location + 70);
	validatePools[(int)NetObjEntityType::Door]				= (TPoolPtr)hook::get_address<void*>(location + 127);
	validatePools[(int)NetObjEntityType::Boat]				= (TPoolPtr)hook::get_address<void*>(location + 184);
	validatePools[(int)NetObjEntityType::Bike]				= (TPoolPtr)hook::get_address<void*>(location + 241);
	validatePools[(int)NetObjEntityType::Automobile]		= (TPoolPtr)hook::get_address<void*>(location + 298);
	validatePools[(int)NetObjEntityType::Pickup]			= (TPoolPtr)hook::get_address<void*>(location + 355);
	validatePools[(int)NetObjEntityType::Train]				= (TPoolPtr)hook::get_address<void*>(location + 453);
	validatePools[(int)NetObjEntityType::Trailer]			= (TPoolPtr)hook::get_address<void*>(location + 508);
	validatePools[(int)NetObjEntityType::Player]			= (TPoolPtr)hook::get_address<void*>(location + 565);
	validatePools[(int)NetObjEntityType::Submarine]			= (TPoolPtr)hook::get_address<void*>(location + 622);
	validatePools[(int)NetObjEntityType::Plane]				= (TPoolPtr)hook::get_address<void*>(location + 672);
	validatePools[(int)NetObjEntityType::PickupPlacement]	= (TPoolPtr)hook::get_address<void*>(location + 722);


	static_assert(offsetof(rage::CNetworkSyncDataULBase, isRemote) == 59, "offset 75");
	static_assert(offsetof(rage::CNetworkSyncDataULBase, creationAckedPlayers) == 96, "offset 112");
	//static_assert(offsetof(CNetGamePlayer, physicalPlayerIndex) == 45 + 8, "offset 45"); // #TODO2060: NOT ANYMORE! HAHA!
	//static_assert(offsetof(CNetGamePlayer, playerInfo) == 168 + 8, "offset 168");
});
