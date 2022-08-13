#include "Log.h"

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>

#define LOG_STR_BUFFER_LEN 1024
#define LOG_TAG            "[Asuka]"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

uint32_t getTid()
{
	return GetCurrentThreadId();
}

void printDebugString(const char* str)
{
	OutputDebugStringA(str);
}

void vlog(const char* level, const char* function, int line, fmt::string_view format, fmt::format_args args)
{
	auto tid     = getTid();
	auto message = fmt::vformat(format, args);
	auto log     = fmt::format("{}:{}:{}:{}:{:04d}:{}\n",
							   LOG_TAG,
							   level,
							   tid,
							   function,
							   line,
							   message);
	printDebugString(log.c_str());
}

void vassert(const char* expression, const char* function, const char* sourcePath, int line, fmt::string_view format, fmt::format_args args)
{
	auto message = fmt::vformat(format, args);
	auto log     = fmt::format("[Assert]: {}\n[Cause]: {}\n[Path]: {}({}): {}\n",
							   expression,
							   message,
							   sourcePath,
							   line,
							   function);
	printDebugString(log.c_str());
#ifdef _DEBUG
	::__debugbreak();
#else
	exit(-1);
#endif  // _DEBUG
}