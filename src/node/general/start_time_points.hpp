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
    uint32_t timestamp() const
    {
        using namespace std::chrono;
        return std::chrono::duration_cast<std::chrono::seconds>(system.time_since_epoch()).count();
    }
};
