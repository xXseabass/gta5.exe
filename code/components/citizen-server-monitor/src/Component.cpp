/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "ComponentLoader.h"

#include <MonitorInstance.h>
#include <Error.h>

class EXPORTED_TYPE ServerComponentInstance : public RunnableComponent
{
private:
	fwRefContainer<fx::MonitorInstance> m_server;

public:
	virtual bool Initialize() override;

	virtual bool SetUserData(const std::string& userData) override;

	virtual bool DoGameLoad(void* module) override;

	virtual bool Shutdown() override;

	virtual void Run() override;
};

bool ServerComponentInstance::Initialize()
{
	return true;
}

bool ServerComponentInstance::SetUserData(const std::string& args)
{
	InitFunctionBase::RunAll();

	m_server = new fx::MonitorInstance();
	return m_server->SetArguments(args);
}

void ServerComponentInstance::Run()
{
	m_server->Run();
}

bool ServerComponentInstance::DoGameLoad(void* module)
{
	FatalError("ServerComponentInstance should not be loaded into a game instance.");

	return false;
}

bool ServerComponentInstance::Shutdown()
{
	return true;
}

extern "C" DLL_EXPORT Component* CreateComponent()
{
	return new ServerComponentInstance();
}
