#pragma once
#include <chrono>

template <typename clock>
struct StartTimePoint : public clock::time_point {
    StartTimePoint()
        : clock::time_point(clock::now())
    {
    }
};

struct StartTimePoints {
    StartTimePoint<std::chrono::steady_clock> steady;
    StartTimePoint<std::chrono::system_clock> system;
};
