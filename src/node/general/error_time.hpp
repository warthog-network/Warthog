#pragma once
#include "general/errors.hpp"
#include <chrono>

struct ErrorTimestamp {
    using steady_clock = std::chrono::steady_clock;
    Error error;
    uint32_t timestamp;
    steady_clock::time_point time_point() const
    {
        using namespace std::chrono;
        return steady_clock::now() + (system_clock::time_point(seconds { timestamp }) - system_clock::now());
    }
};

struct ErrorTimepoint {
    using steady_clock = std::chrono::steady_clock;
    using time_point = steady_clock::time_point;
    // data
    Error error;
    time_point timepoint;

    ErrorTimepoint(Error error, time_point timepoint)
        : error(error)
        , timepoint(timepoint)
    {
    }
    ErrorTimepoint(const ErrorTimestamp& et)
        : ErrorTimepoint(et.error, et.time_point())
    {
    }

    static ErrorTimepoint from_duration(Error e, steady_clock::duration duration)
    {
        return ErrorTimepoint { e, steady_clock::now() + duration };
    }

    operator ErrorTimestamp() const
    {
        return { error, timestamp() };
    }

    uint32_t timestamp() const
    {
        using namespace std::chrono;
        return duration_cast<seconds>((system_clock::now() + (timepoint - steady_clock::now())).time_since_epoch()).count();
    }
};
