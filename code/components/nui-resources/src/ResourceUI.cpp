/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "ResourceUI.h"
#include <include/cef_origin_whitelist.h>

#include <NUISchemeHandlerFactory.h>

#include <ResourceMetaDataComponent.h>
#include <ResourceGameLifetimeEvents.h>

#include <ResourceManager.h>

#include <boost/algorithm/string/predicate.hpp>
#include <mutex>

ResourceUI::ResourceUI(Resource* resource)
	: m_resource(resource), m_hasFrame(false), m_hasCallbacks(false)
{

}

ResourceUI::~ResourceUI()
{

}

bool ResourceUI::Create()
{
	// initialize callback handlers
	auto resourceName = m_resource->GetName();
	std::transform(resourceName.begin(), resourceName.end(), resourceName.begin(), ::ToLower);
	nui::RegisterSchemeHandlerFactory("http", resourceName, Instance<NUISchemeHandlerFactory>::Get());
	nui::RegisterSchemeHandlerFactory("https", resourceName, Instance<NUISchemeHandlerFactory>::Get());

	// get the metadata component
	fwRefContainer<fx::ResourceMetaDataComponent> metaData = m_resource->GetComponent<fx::ResourceMetaDataComponent>();

	// get the UI page list and a number of pages
	auto uiPageData = metaData->GetEntries("ui_page");
	int pageCount = std::distance(uiPageData.begin(), uiPageData.end());

	// if no page exists, return
	if (pageCount == 0)
	{
		return false;
	}

	// if more than one, warn
	if (pageCount > 1)
	{
		trace(__FUNCTION__ ": more than one ui_page in resource %s\n", m_resource->GetName().c_str());
		return false;
	}

	// mark us as having a frame
	m_hasFrame = true;

	// get the page name from the iterator
	std::string pageName = uiPageData.begin()->second;
	nui::RegisterSchemeHandlerFactory("https", "cfx-nui-" + resourceName, Instance<NUISchemeHandlerFactory>::Get());

	// create the NUI frame
	auto rmvRes = metaData->IsManifestVersionBetween("cerulean", "");
	auto uiPrefix = (!rmvRes || !*rmvRes) ? "nui://" : "https://cfx-nui-";

	std::string path = uiPrefix + m_resource->GetName() + "/" + pageName;

	// allow direct mentions of absolute URIs
	if (pageName.find("://") != std::string::npos)
	{
		path = pageName;
	}

	auto uiPagePreloadData = metaData->GetEntries("ui_page_preload");
	bool uiPagePreload = std::distance(uiPagePreloadData.begin(), uiPagePreloadData.end()) > 0;

	if (uiPagePreload)
	{
		nui::CreateFrame(m_resource->GetName(), path);
	}
	else
	{
		nui::PrepareFrame(m_resource->GetName(), path);
	}

	// add a cross-origin entry to allow fetching the callback handler
	CefAddCrossOriginWhitelistEntry(va("nui://%s", m_resource->GetName().c_str()), "http", m_resource->GetName(), true);
	CefAddCrossOriginWhitelistEntry(va("nui://%s", m_resource->GetName().c_str()), "https", m_resource->GetName(), true);
	
	CefAddCrossOriginWhitelistEntry(va("https://cfx-nui-%s", m_resource->GetName().c_str()), "https", m_resource->GetName(), true);
	CefAddCrossOriginWhitelistEntry(va("https://cfx-nui-%s", m_resource->GetName().c_str()), "nui", m_resource->GetName(), true);

	return true;
}

void ResourceUI::Destroy()
{
	// destroy the target frame
	nui::DestroyFrame(m_resource->GetName());
}

void ResourceUI::AddCallback(const std::string& type, ResUICallback callback)
{
	m_callbacks.insert({ type, callback });
}

void ResourceUI::RemoveCallback(const std::string& type)
{
	// Note: This is called by UNREGISTER_RAW_NUI_CALLBACK but
	// can still technically target event based NUI Callbacks
	m_callbacks.erase(type);
}

bool ResourceUI::InvokeCallback(const std::string& type, const std::string& query, const std::multimap<std::string, std::string>& headers, const std::string& data, ResUIResultCallback resultCB)
{
	auto set = fx::GetIteratorView(m_callbacks.equal_range(type));

	if (set.begin() == set.end())
	{
		// try mapping only the first part
		auto firstType = type.substr(0, type.find_first_of('/'));

		set = fx::GetIteratorView(m_callbacks.equal_range(firstType));

		if (set.begin() == set.end())
		{
			return false;
		}
	}

	for (auto& cb : set)
	{
		cb.second(type, query, headers, data, resultCB);
	}

	return true;
}

void ResourceUI::SignalPoll()
{
	nui::SignalPoll(m_resource->GetName());
}

static std::map<std::string, fwRefContainer<ResourceUI>> g_resourceUIs;
static std::mutex g_resourceUIMutex;

#include <boost/algorithm/string.hpp>

static bool NameMatches(std::string_view name, std::string_view match)
{
	if (name == match)
	{
		return true;
	}

	if (boost::algorithm::starts_with(name, match))
	{
		if (name[match.length()] == '/')
		{
			return true;
		}
	}

	return false;
}

static InitFunction initFunction([] ()
{
	fx::ResourceManager::OnInitializeInstance.Connect([](fx::ResourceManager* manager)
	{
		nui::SetResourceLookupFunction([manager](const std::string& resourceName, const std::string& fileName) -> std::string
		{
			fwRefContainer<fx::Resource> resource;

			fx::ResourceManager* resourceManager = Instance<fx::ResourceManager>::Get();
			resourceManager->ForAllResources([&resourceName, &resource](const fwRefContainer<fx::Resource>& resourceRef)
			{
				if (_stricmp(resourceRef->GetName().c_str(), resourceName.c_str()) == 0)
				{
					resource = resourceRef;
				}
			});

			if (resource.GetRef())
			{
				auto path = resource->GetPath();

				if (!boost::algorithm::ends_with(path, "/"))
				{
					path += "/";
				}

				// check if it's a client script of any sorts
				std::stringstream normalFileName;
				char lastC = '/';

				for (size_t i = 0; i < fileName.length(); i++)
				{
					char c = fileName[i];

					if (c != '/' || lastC != '/')
					{
						normalFileName << c;
					}

					lastC = c;
				}

				auto nfn = normalFileName.str();

				auto mdComponent = resource->GetComponent<fx::ResourceMetaDataComponent>();
				bool valid = false;

				if (NameMatches(nfn, "__resource.lua") || NameMatches(nfn, "fxmanifest.lua"))
				{
					return "common:/data/gameconfig.xml";
				}
				
				for (auto& entry : mdComponent->GlobEntriesVector("client_script"))
				{
					if (NameMatches(nfn, entry))
					{
						auto files = mdComponent->GlobEntriesVector("file");
						bool isFile = false;

						for (auto& fileEntry : files)
						{
							if (NameMatches(nfn, fileEntry))
							{
								isFile = true;
								break;
							}
						}

						if (!isFile)
						{
							return "common:/data/gameconfig.xml";
						}
					}
				}

				return path + fileName;
			}

			return fmt::sprintf("resources:/%s/%s", resourceName, fileName);
		});
	});

	Resource::OnInitializeInstance.Connect([] (Resource* resource)
	{
		// create the UI instance
		fwRefContainer<ResourceUI> resourceUI(new ResourceUI(resource));

		// start event
		resource->OnCreate.Connect([=] ()
		{
			std::unique_lock<std::mutex> lock(g_resourceUIMutex);

			resourceUI->Create();
			g_resourceUIs[resource->GetName()] = resourceUI;
		});

		// stop event
		resource->OnStop.Connect([=] ()
		{
			std::unique_lock<std::mutex> lock(g_resourceUIMutex);

			if (g_resourceUIs.find(resource->GetName()) != g_resourceUIs.end())
			{
				resourceUI->Destroy();
				g_resourceUIs.erase(resource->GetName());
			}
		});

#ifdef GTA_FIVE
		// pre-disconnect handling
		resource->GetComponent<fx::ResourceGameLifetimeEvents>()->OnBeforeGameShutdown.Connect([=]()
		{
			std::unique_lock<std::mutex> lock(g_resourceUIMutex);

			if (g_resourceUIs.find(resource->GetName()) != g_resourceUIs.end())
			{
				resourceUI->Destroy();
				g_resourceUIs.erase(resource->GetName());
			}
		});
#endif

		// add component
		resource->SetComponent(resourceUI);
	});
});

#include <CefOverlay.h>
#include <HttpClient.h>

static InitFunction httpInitFunction([]()
{
	nui::RequestNUIBlocklist.Connect([](std::function<void(bool, const char*, size_t)> cb)
	{
		auto httpClient = Instance<HttpClient>::Get();
		httpClient->DoGetRequest("https://runtime.fivem.net/nui-blacklist.json", cb);
	});
});
