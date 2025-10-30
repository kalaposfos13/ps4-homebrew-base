#pragma once

#include "fmt/format.h"

extern "C" void sceSysUtilSendSystemNotificationWithText(int type, const char* message);

template <typename... Args>
std::string FormatLog(char const* format, Args const&... args) {
    std::string message;
    if constexpr (sizeof...(args) == 0) {
        message = format;
    } else {
        message = fmt::vformat(format, fmt::make_format_args(args...));
    }
    return message;
}

template <typename... Args>
void PrintLog(char const* log_level, char const* file, unsigned int line_num, char const* function,
              char const* format, Args const&... args) {
    std::string message = FormatLog(format, args...);
    std::string full_log =
        fmt::format("[Homebrew] {}:{} <{}> {}: {}\n", file, line_num, log_level, function, message);
    sceKernelDebugOutText(0, full_log.c_str());
}

template <typename... Args>
void PrintLogN(char const* log_level, char const* file, unsigned int line_num, char const* function,
               char const* format, Args const&... args) {
    PrintLog(log_level, file, line_num, function, format, args...);
    std::string message = FormatLog(format, args...);
    sceSysUtilSendSystemNotificationWithText(222, message.c_str());
}

#define LOG_DEBUG(...) PrintLog("Debug", __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...) PrintLog("Info", __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARNING(...) PrintLog("Warning", __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(...) PrintLog("Error", __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_CRITICAL(...) PrintLog("Critical", __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_NOTIFICATION(...) PrintLogN("Notification", __FILE__, __LINE__, __func__, __VA_ARGS__)
