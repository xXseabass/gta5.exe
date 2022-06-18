#include "StdInc.h"
#include "GameFlags.h"

static std::unordered_map<GameFlag, bool> g_gameFlags;

void GameFlags::ResetFlags()
{
	SetFlag(GameFlag::PlayerActivated, false);
	SetFlag(GameFlag::NetworkWalkMode, true);
	SetFlag(GameFlag::InstantSendPackets, true);
}

bool GameFlags::GetFlag(GameFlag flag)
{
	return g_gameFlags[flag];
}

void GameFlags::SetFlag(GameFlag flag, bool value)
{
	g_gameFlags[flag] = value;
}