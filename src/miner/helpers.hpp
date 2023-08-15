#pragma once
#include<string_view>
#include<utility>
#include "spdlog/formatter.h"
inline std::pair<double, std::string_view> format_hashrate(size_t hr)
{
    using namespace std::literals;
    if (hr > 1000000000)
        return { double(hr) / 1000000000, "gh"sv };
    if (hr > 1000000)
        return { double(hr) / 1000000, "mh"sv };
    if (hr > 1000)
        return { double(hr) / 1000, "kh"sv };
    return { hr, "h"sv };
}

inline std::string format_duration(uint32_t seconds)
{
    if (seconds < 300) {
        return spdlog::fmt_lib::format("{} seconds", seconds);
    }
    if (seconds < 60 * 60)
        return spdlog::fmt_lib::format("{} minutes", uint32_t(10 * double(seconds) / 60.0) / 10.0);
    return spdlog::fmt_lib::format("{} hours", int32_t(10 * double(seconds) / 3600.0) / 10.0);
}

inline uint32_t randuint32()
{
    uint32_t v;
    uint8_t* p = reinterpret_cast<uint8_t*>(&v);
    p[0] = rand() % 256;
    p[1] = rand() % 256;
    p[2] = rand() % 256;
    p[3] = rand() % 256;
    return v;
}
