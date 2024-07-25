#pragma once

#include <chrono>
#include <algorithm>
class RateLimiter {
    using time_point = std::chrono::steady_clock::time_point;
    using duration_t = std::chrono::steady_clock::duration;

    std::chrono::steady_clock::time_point min_time() const
    {
        return std::chrono::steady_clock::now() - d;
    }

public:
    RateLimiter(duration_t d, size_t N)
        : d(d)
        , N(N)
    {
        tp = min_time();
    }
    void count_event()
    {
        tp = std::max(min_time(), tp) + d / N;
    }
    auto cooldown_tp() const {
        return tp + d;
    }
    bool limited() const{
        return tp > std::chrono::steady_clock::now();
    }

private:
    std::chrono::steady_clock::duration d;
    size_t N { 10 };
    time_point tp;
};
