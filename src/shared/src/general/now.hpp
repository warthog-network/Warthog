#pragma once

#include <chrono>
inline uint32_t now_timestamp()
{
    using namespace std::chrono;
    return std::chrono::seconds(std::time(NULL)).count();
}
