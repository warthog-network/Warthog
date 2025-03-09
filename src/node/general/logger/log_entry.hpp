#pragma once
#include <string>
#include "spdlog/common.h"

struct LogEntry {
    spdlog::level::level_enum level;
    spdlog::log_clock::time_point tp;
    std::string payload;
    std::string datetime;
};
