#include <StdInc.h>
#include "Console.Base.h"

#include "Console.VariableHelpers.h"

#include <stdarg.h>
#include <CL2LaunchMode.h>

#include <tbb/concurrent_queue.h>
#include <boost/algorithm/string/replace.hpp>

#include <condition_variable>

namespace console
{
static bool g_allowVt;

static inline bool _isdigit(char c)
{
	return (c >= '0' && c <= '9');
}

static const int g_colors[] = {
	97, // bright white, black
	91, // red
	32, // green
	93, // bright yellow
	94, // blue
	36, // cyan
	35, // magenta
	0,  // reset
	31, // dark red
	34  // dark blue
};

static void Print(const char* str)
{
	if (launch::IsSDK() || launch::IsSDKGuest())
	{
		fprintf(stderr, "%s", str);
	}
	else
	{
		printf("%s", str);
	}
}

static auto g_printf = &Print;
static thread_local std::string g_printChannel;
static bool g_startOfLine = true;
static int lastColor = 7;

template<typename Stream>
inline void WriteColor(Stream& buf, int color)
{
	buf << fmt::sprintf("\x1B[%dm", g_colors[color]);
}

template<typename Stream>
inline void WriteChannel(Stream& buf, const std::string& channelName)
{
	if (g_allowVt)
	{
		auto hash = HashString(channelName.c_str());
		buf << fmt::sprintf("\x1B[38;5;%dm", (hash % (231 - 17)) + 17);
	}

	std::string effectiveChannelName = channelName;

	if (channelName.length() > 20)
	{
		if (channelName.find("citizen-") == 0)
		{
			effectiveChannelName = "c-" + channelName.substr(8);
		}
	}

	buf << fmt::sprintf("[%20s] ", effectiveChannelName.substr(0, std::min(size_t(20), effectiveChannelName.length())));

	if (g_allowVt)
	{
		buf << fmt::sprintf("\x1B[%dm", 0);
	}
}

static void CfxPrintf(const std::string& str)
{
	std::stringstream buf;

	for (size_t i = 0; i < str.size(); i++)
	{
		if (g_startOfLine)
		{
			WriteChannel(buf, g_printChannel);
			WriteColor(buf, lastColor);
			g_startOfLine = false;
		}

		if (str[i] == '^' && _isdigit(str[i + 1]))
		{
			if (g_allowVt)
			{
				lastColor = str[i + 1] - '0';
				WriteColor(buf, lastColor);
			}

			i += 1;
		}
		else
		{
			buf << fmt::sprintf("%c", str[i]);
			if (str[i] == '\n')
			{
				g_startOfLine = true;
			}
		}
	}

	g_printf(buf.str().c_str());
}

static void PrintfTraceListener(ConsoleChannel channel, const char* out);

static struct consoleBase
{
	std::once_flag initConsoleFlag;
	std::condition_variable consoleCondVar;
	std::mutex consoleMutex;

	tbb::concurrent_queue<std::tuple<std::string, std::string>> consolePrintQueue;
	bool isPrinting = false;

	fwEvent<ConsoleChannel, const char*> printFilter;
	std::vector<void (*)(ConsoleChannel, const char*)> printListeners = { PrintfTraceListener };
	int useDeveloper = 0;
}* gConsole = new consoleBase();

static void PrintfTraceListener(ConsoleChannel channel, const char* out)
{
#ifdef _WIN32
	static std::once_flag g_vtOnceFlag;

	std::call_once(g_vtOnceFlag, []()
	{
		{
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

			DWORD consoleMode;
			if (GetConsoleMode(hConsole, &consoleMode))
			{
				consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

				if (SetConsoleMode(hConsole, consoleMode))
				{
					g_allowVt = true;
				}

				if (IsWindows10OrGreater())
				{
					SetConsoleCP(65001);
					SetConsoleOutputCP(65001);
				}
			}
			else
			{
				g_allowVt = true;
			}
		}
	});
#else
	g_allowVt = true;
#endif

	std::call_once(gConsole->initConsoleFlag, []()
	{
		std::thread([]()
		{
			SetThreadName(-1, "[Cfx] Console Thread");

			while (true)
			{
				{
					std::unique_lock<std::mutex> lock(gConsole->consoleMutex);
					gConsole->consoleCondVar.wait(lock);
				}

				std::tuple<std::string, std::string> strRef;

				while (gConsole->consolePrintQueue.try_pop(strRef))
				{
					gConsole->isPrinting = true;
					g_printChannel = std::get<0>(strRef);
					CfxPrintf(std::get<1>(strRef));
					g_printChannel = "";
					gConsole->isPrinting = false;
				}
			}
		}).detach();
	});

	gConsole->consolePrintQueue.push({ channel, std::string{ out } });
	gConsole->consoleCondVar.notify_all();
}
}

bool GIsPrinting()
{
	return !console::gConsole->consolePrintQueue.empty() || console::gConsole->isPrinting;
}

namespace console
{
void Printfv(ConsoleChannel channel, std::string_view format, fmt::printf_args argList)
{
	auto buffer = fmt::vsprintf(format, argList);

	// replace NUL characters, if any
	if (buffer.find('\0') != std::string::npos)
	{
		boost::algorithm::replace_all(buffer, std::string_view{ "\0", 1 }, "^5<\\0>^7");
	}

	// run print filter
	if (!gConsole->printFilter(channel, buffer.data()))
	{
		return;
	}

	// print to all interested listeners
	for (auto& listener : gConsole->printListeners)
	{
		listener(channel, buffer.data());
	}
}

void DPrintfv(ConsoleChannel channel, std::string_view format, fmt::printf_args argList)
{
	if (gConsole->useDeveloper > 0)
	{
		Printfv(channel, format, argList);
	}
}

static void AddColorTerminator(std::string& str)
{
	auto insertionPoint = str.find_last_not_of('\n');
	if (insertionPoint != std::string::npos)
	{
		str.insert(insertionPoint + 1, "^7");
	}
}

void PrintWarningv(ConsoleChannel channel, std::string_view format, fmt::printf_args argList)
{
	std::string warningText = "^3Warning: " + fmt::vsprintf(format, argList);
	AddColorTerminator(warningText);
	Printf(channel, "%s", warningText);
}

void PrintErrorv(ConsoleChannel channel, std::string_view format, fmt::printf_args argList)
{
	std::string errorText = "^1Error: " + fmt::vsprintf(format, argList);
	AddColorTerminator(errorText);
	Printf(channel, "%s", errorText);
}

static ConVar<int> developerVariable(GetDefaultContext(), "developer", ConVar_Archive, 0, &gConsole->useDeveloper);
}

extern "C" DLL_EXPORT void CoreAddPrintListener(void(*function)(ConsoleChannel, const char*))
{
	console::gConsole->printListeners.push_back(function);
}

extern "C" DLL_EXPORT fwEvent<ConsoleChannel, const char*>* CoreGetPrintFilterEvent()
{
	return &console::gConsole->printFilter;
}

extern "C" DLL_EXPORT void CoreSetPrintFunction(void(*function)(const char*))
{
	console::g_printf = function;
}
