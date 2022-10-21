#pragma once

#include <fmt/format.h>

#define LOG_LEVEL_DEBUG   "DEBUG"
#define LOG_LEVEL_TRACE   "TRACE"
#define LOG_LEVEL_FIXME   "FIXME"
#define LOG_LEVEL_WARNING "WARNING"
#define LOG_LEVEL_ERROR   "ERROR"

void vlog(const char* level, const char* function, int line, fmt::string_view format, fmt::format_args args);
void vassert(const char* expression, const char* function, const char* sourcePath, int line, fmt::string_view format, fmt::format_args args);

template <typename S, typename... Args>
void logPrint(const char* level, const char* function, int line, const S& format, Args&&... args)
{
	vlog(level, function, line, format,
		 fmt::make_format_args(args...));
}

template <typename S, typename... Args>
void logAssert(const char* expression, const char* function, const char* sourcePath, int line, const S& format, Args&&... args)
{
	vassert(expression, function, sourcePath, line, format,
			fmt::make_format_args(args...));
}

#define _LOG_PRINT_(level, format, ...) logPrint(level, __FUNCTION__, __LINE__, format, __VA_ARGS__)
#define _LOG_ASSERT_(expr, format, ...) (void)(!!(expr) || (logAssert(#expr, __FUNCTION__, __FILE__, __LINE__, format, __VA_ARGS__), 0))


#ifdef _DEBUG

#define LOG_DEBUG(format, ...)        _LOG_PRINT_(LOG_LEVEL_DEBUG, format, __VA_ARGS__)
#define LOG_TRACE(format, ...)        _LOG_PRINT_(LOG_LEVEL_TRACE, format, __VA_ARGS__)
#define LOG_FIXME(format, ...)        _LOG_PRINT_(LOG_LEVEL_FIXME, format, __VA_ARGS__)
#define LOG_WARN(format, ...)         _LOG_PRINT_(LOG_LEVEL_WARNING, format, __VA_ARGS__)
#define LOG_ERR(format, ...)          _LOG_PRINT_(LOG_LEVEL_ERROR, format, __VA_ARGS__)
#define LOG_ASSERT(expr, format, ...) _LOG_ASSERT_(expr, format, __VA_ARGS__)

#else

#define LOG_DEBUG(format, ...)
#define LOG_TRACE(format, ...)
#define LOG_FIXME(format, ...)
#define LOG_WARN(format, ...)
#define LOG_ERR(format, ...)
#define LOG_ASSERT(expr, format, ...)

#endif