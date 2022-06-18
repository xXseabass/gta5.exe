/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include <StdInc.h>

#if defined(GTA_FIVE)
#include <ScriptEngine.h>

#include <Resource.h>
#include <fxScripting.h>

#include <DisableChat.h>
#include <nutsnbolts.h>

#include <ICoreGameInit.h>

static InitFunction initFunction([] ()
{
	static std::set<std::string> g_textChatDisableResources;

	Instance<ICoreGameInit>::Get()->OnGameRequestLoad.Connect([]()
	{
		g_textChatDisableResources.clear();
	});

	// disable legacy text chat by default, for it is confusing
	OnMainGameFrame.Connect([]()
	{
		if (Instance<ICoreGameInit>::Get()->HasVariable("storyMode"))
		{
			return;
		}

		static bool chatOff = false;

		if (!chatOff && Instance<ICoreGameInit>::Get()->HasVariable("gameSettled"))
		{
			game::SetTextChatEnabled(false);
			chatOff = true;
		}
		else if (chatOff && !Instance<ICoreGameInit>::Get()->HasVariable("gameSettled"))
		{
			chatOff = false;
		}
	});

	fx::ScriptEngine::RegisterNativeHandler("SET_TEXT_CHAT_ENABLED", [] (fx::ScriptContext& context)
	{
		fx::OMPtr<IScriptRuntime> runtime;

		if (FX_SUCCEEDED(fx::GetCurrentScriptRuntime(&runtime)))
		{
			fx::Resource* resource = reinterpret_cast<fx::Resource*>(runtime->GetParentObject());

			auto tryEnableForResource = [=] ()
			{
				// erase the resource and see if the list is empty - if so, enable chat
				g_textChatDisableResources.erase(resource->GetName());

				// check if empty
				if (g_textChatDisableResources.empty())
				{
					game::SetTextChatEnabled(true);
				}
			};

			if (resource)
			{
				bool enabled = context.GetArgument<bool>(0);

				// if it's already disabled by this resource
				if (g_textChatDisableResources.find(resource->GetName()) != g_textChatDisableResources.end())
				{
					// if we want to enable it
					if (enabled)
					{
						tryEnableForResource();
					}
				}
				else
				{
					// if we want to disable it
					if (!enabled)
					{
						// and we're the first to do so
						if (g_textChatDisableResources.empty())
						{
							// disable it
							game::SetTextChatEnabled(false);
						}

						// mark us as being 'involved' in disabling text chat
						g_textChatDisableResources.insert(resource->GetName());

						// on-stop handler, as well, to remove from the list when the resource stops
						resource->OnStop.Connect([=] ()
						{
							// check if we're still in there
							if (g_textChatDisableResources.find(resource->GetName()) != g_textChatDisableResources.end())
							{
								// do enabling stuff
								tryEnableForResource();
							}
						});
					}
				}
			}
		}

		context.SetResult<bool>(g_textChatDisableResources.empty());
	});
});
#endif
