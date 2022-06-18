#include <StdInc.h>

#ifdef _M_AMD64
#include <ipfs_lite.grpc.pb.h>

#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <grpc-ipfs.h>

#include <tbb/concurrent_queue.h>

using DownloadCb = bool(*)(void* cxt, const void* data, size_t size);
using FinishCb = void(*)(void* cxt, const char* error);

namespace ipfsdl
{
	static tbb::concurrent_queue<std::function<void()>> g_pollQueue;
	static std::unique_ptr<ipfs_lite::IpfsLite::Stub> g_ipfs;
	static HMODULE hModule;

	static bool StartInner()
	{
		hModule = LoadLibrary(L"grpc-ipfs.dll");

		if (!hModule)
		{
			return false;
		}

		std::string swarmKeyRaw = R"(/key/swarm/psk/1.0.0/
/base16/
fdb4684a58cda3f34aa289e603751631e6526f4ca332e876eca163b6b91a37d7)";

		auto start = (decltype(&::Start))GetProcAddress(hModule, "Start");
		auto rv = start(ToNarrow(MakeRelativeCitPath(L"data/ipfs-r2/")).data(), swarmKeyRaw.data(), swarmKeyRaw.size(), 0);

		if (rv.r1)
		{
			trace("IPFS downloader init failure: %s\n", rv.r1);
			return false;
		}

		auto channel = grpc::CreateChannel(fmt::sprintf("localhost:%d", rv.r0), grpc::InsecureChannelCredentials());
		g_ipfs = ipfs_lite::IpfsLite::NewStub(channel);

		return true;
	}

	void Start(void(*cb)(bool))
	{
		std::thread([cb]
		{
			bool rv = StartInner();
			g_pollQueue.push([cb, rv]()
			{
				cb(rv);
			});
		}).detach();
	}

	bool DownloadFile(void* cxt, const char* url, DownloadCb cb, FinishCb done)
	{
		std::string cid = &url[7];

		std::thread([cxt, cid, cb, done]()
		{
			static volatile bool cancel;

			ipfs_lite::GetFileRequest req;
			req.set_cid(cid);

			grpc::ClientContext context;
			auto responses = g_ipfs->GetFile(&context, req);

			ipfs_lite::GetFileResponse response;
			cancel = false;

			while (responses->Read(&response))
			{
				if (cancel)
				{
					return;
				}

				auto chunk = response.chunk();

				g_pollQueue.push([chunk, cxt, cb, done]()
				{
					if (!cb(cxt, chunk.data(), chunk.size()))
					{
						cancel = true;
						done(cxt, "Write callback returned false.");
					}
				});
			}

			auto s = responses->Finish();

			g_pollQueue.push([s, cxt, done]()
			{
				if (s.ok())
				{
					done(cxt, nullptr);
				}
				else
				{
					done(cxt, s.error_message().c_str());
				}
			});
		}).detach();

		return true;
	}

	bool Exit()
	{
		auto stop = (decltype(&::Stop))GetProcAddress(hModule, "Stop");

		if (stop)
		{
			return stop() == nullptr;
		}

		return false;
	}

	void Poll()
	{
		std::function<void()> fn;

		while (g_pollQueue.try_pop(fn))
		{
			fn();
		}
	}
}

extern "C" DLL_EXPORT void ipfsdlInit(void(*cb)(bool))
{
	return ipfsdl::Start(cb);
}

extern "C" DLL_EXPORT bool ipfsdlDownloadFile(void* cxt, const char* url, DownloadCb cb, FinishCb done)
{
	return ipfsdl::DownloadFile(cxt, url, cb, done);
}

extern "C" DLL_EXPORT bool ipfsdlExit()
{
	return ipfsdl::Exit();
}

extern "C" DLL_EXPORT void ipfsdlPoll()
{
	return ipfsdl::Poll();
}
#endif
