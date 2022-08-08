#include <Windows.h>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/msvc_sink.h"
#include "detours.h"
#include <string>
#include <set>




typedef HANDLE(WINAPI* PFUNC_CreateFileW)(
	LPCWSTR               lpFileName,
	DWORD                 dwDesiredAccess,
	DWORD                 dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD                 dwCreationDisposition,
	DWORD                 dwFlagsAndAttributes,
	HANDLE                hTemplateFile);

PFUNC_CreateFileW g_OldCreateFileW = CreateFileW;

HANDLE WINAPI NewCreateFileW(
	LPCWSTR               lpFileName,
	DWORD                 dwDesiredAccess,
	DWORD                 dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD                 dwCreationDisposition,
	DWORD                 dwFlagsAndAttributes,
	HANDLE                hTemplateFile)
{
	static std::set<std::wstring> s_nameSet;

	HANDLE handle = g_OldCreateFileW(lpFileName,
					 dwDesiredAccess,
					 dwShareMode,
					 lpSecurityAttributes,
					 dwCreationDisposition,
					 dwFlagsAndAttributes,
					 hTemplateFile);

	do 
	{
		if (handle == INVALID_HANDLE_VALUE)
		{
			break;
		}

		std::wstring fileName(lpFileName);
		if (fileName[0] == '\\')
		{
			break;
		}

		if (s_nameSet.find(fileName) == s_nameSet.end())
		{
			auto logger = spdlog::get("gow-logger");
			if (logger)
			{
				logger->info(L"{}", fileName);
				logger->flush();
			}

			s_nameSet.insert(fileName);
		}

	} while (false);

	return handle;
}


void InitProc()
{
	auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("gow-log.txt", true);
	spdlog::sinks_init_list sink_list = { file_sink, msvc_sink };

	auto logger = std::make_shared<spdlog::logger>("gow-logger", sink_list.begin(), sink_list.end());
	spdlog::set_default_logger(logger);
	spdlog::flush_on(spdlog::level::info);
	spdlog::set_pattern("[Asuka] [%t] %v");

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourAttach((void**)&g_OldCreateFileW, NewCreateFileW);
  
    DetourTransactionCommit();
}

void UnProc()
{

}


__declspec(dllexport)void WINAPI Dummy()
{
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        InitProc();
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        UnProc();
        break;
    }
    return TRUE;
}
