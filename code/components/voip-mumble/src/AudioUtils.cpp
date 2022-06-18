#include "StdInc.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl.h>

#include <audiopolicy.h>

namespace WRL = Microsoft::WRL;

/*extern "C" const PROPERTYKEY PKEY_AudioEndpoint_GUID = { {
		0x1da5d803, 0xd492, 0x4edd,{
	0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e
} }, 4
};*/

static HRESULT GetStringProp(IPropertyStore* bag, PROPERTYKEY key, std::string* out)
{
	out->clear();
	PROPVARIANT var;
	PropVariantInit(&var);
	HRESULT hr = bag->GetValue(key, &var);
	if (SUCCEEDED(hr))
	{
		if (var.pwszVal)
		{
			*out = ToNarrow(var.pwszVal);
		}
		else
		{
			hr = E_FAIL;
		}
	}
	PropVariantClear(&var);
	return hr;
}

WRL::ComPtr<IMMDevice> GetMMDeviceFromGUID(bool input, const std::string& guid)
{
	WRL::ComPtr<IMMDeviceEnumerator> mmDeviceEnumerator;
	HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_INPROC_SERVER, IID_IMMDeviceEnumerator, (void**)mmDeviceEnumerator.GetAddressOf());

	if (SUCCEEDED(hr))
	{
		WRL::ComPtr<IMMDeviceCollection> devices;
		hr = mmDeviceEnumerator->EnumAudioEndpoints(input ? eCapture : eRender, DEVICE_STATE_ACTIVE, &devices);

		if (SUCCEEDED(hr))
		{
			uint32_t count;
			hr = devices->GetCount(&count);

			if (SUCCEEDED(hr))
			{
				for (uint32_t i = 0; i < count; i++)
				{
					WRL::ComPtr<IMMDevice> device;
					hr = devices->Item(i, device.ReleaseAndGetAddressOf());

					if (FAILED(hr))
					{
						break;
					}

					WRL::ComPtr<IPropertyStore> propertyStore;
					hr = device->OpenPropertyStore(STGM_READ, &propertyStore);

					if (FAILED(hr))
					{
						continue;
					}

					std::string testGuid;
					hr = GetStringProp(propertyStore.Get(), PKEY_AudioEndpoint_GUID, &testGuid);

					if (FAILED(hr))
					{
						continue;
					}

					std::string friendlyName;
					hr = GetStringProp(propertyStore.Get(), PKEY_Device_FriendlyName, &friendlyName);

					if (FAILED(hr))
					{
						continue;
					}

					// if the guid matches, return the device
					if (testGuid == guid)
					{
						trace("Returning device %s for GUID %s\n", friendlyName, testGuid);
						return device;
					}
				}
			}
		}
	}

	return nullptr;
}

void DuckingOptOutInner(const WRL::ComPtr<IMMDevice>& device)
{
	WRL::ComPtr<IAudioSessionManager2> sessionManager;

	if (SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, NULL, &sessionManager)))
	{
		WRL::ComPtr<IAudioSessionControl> sessionControl;
		WRL::ComPtr<IAudioSessionControl2> sessionControl2;

		if (SUCCEEDED(sessionManager->GetAudioSessionControl(NULL, 0, &sessionControl)))
		{
			if (SUCCEEDED(sessionControl.As(&sessionControl2)))
			{
				sessionControl2->SetDuckingPreference(TRUE);
			}
		}
	}
}

// SEH wrapper to prevent crashing from a Steam 'gameoverlayrenderer' bug
void DuckingOptOutOuter(const WRL::ComPtr<IMMDevice>& device)
{
	__try
	{
		DuckingOptOutInner(device);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	
	}
}

void DuckingOptOut(WRL::ComPtr<IMMDevice> device)
{
	DuckingOptOutOuter(device);
}

std::mutex g_mmDeviceMutex;
