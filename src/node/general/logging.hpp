#pragma once
#include "global/globals.hpp"

#include "spdlog/spdlog.h"
template <typename... Args>
inline void log_rtc(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
    if (config().logRTC)
        spdlog::info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_communication(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
    if (config().logCommunication)
        spdlog::info(fmt, std::forward<Args>(args)...);
}
