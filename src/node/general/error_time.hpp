#pragma once
#include "general/errors.hpp"
#include "general/timestamp.hpp"
#include <chrono>

struct ErrorTimestamp {
    using steady_clock = std::chrono::steady_clock;
    Error error;
    Timestamp timestamp;
    steady_clock::time_point time_point() const
    {
        return timestamp.steady_clock_time_point();
    }
};

struct ErrorTimepoint : public Timepoint {
    using steady_clock = std::chrono::steady_clock;
    using tp = steady_clock::time_point;
    // data
    Error error;

    ErrorTimepoint(Error error, tp timepoint)
        : Timepoint(timepoint)
        , error(error)
    {
    }
    ErrorTimepoint(Error error, Timestamp timestamp)
        : ErrorTimepoint(error, timestamp.steady_clock_time_point())
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
    tp& time_point()
    {
        return *static_cast<tp*>(this);
    }
};
