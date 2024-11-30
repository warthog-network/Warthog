#pragma once

#include <chrono>
// #include <compare>
// #include <cstdint>
class Timestamp {

public:
    Timestamp(uint32_t v)
        : data(v)
    {
    }
    using time_point = std::chrono::steady_clock::time_point;
    auto operator<=>(const Timestamp&) const = default;
    static Timestamp now()
    {
        using namespace std::chrono;
        return Timestamp(duration_cast<seconds>(steady_clock::now().time_since_epoch()).count());
    }
    static Timestamp from_time_point(time_point tp)
    {
        using namespace std::chrono;
        return duration_cast<seconds>((system_clock::now() + (tp - steady_clock::now())).time_since_epoch()).count();
    }

    std::chrono::steady_clock::time_point steady_clock_time_point() const
    {
        using namespace std::chrono;
        return steady_clock::now() + (system_clock::time_point(seconds(data)) - system_clock::now());
    }
    Timestamp operator+(std::chrono::steady_clock::duration d)
    {
        using namespace std::chrono;
        return Timestamp(data + duration_cast<seconds>(d).count());
    }
    auto val() const { return data; }

private:
    uint32_t data;
};

struct Timepoint : public std::chrono::steady_clock::time_point {
    Timepoint(std::chrono::steady_clock::time_point tp)
        : std::chrono::steady_clock::time_point(tp)
    {
    }
    static Timepoint now()
    {
        return { std::chrono::steady_clock::now() };
    }
    Timestamp timestamp() const
    {
        using namespace std::chrono;
        return duration_cast<seconds>((system_clock::now() + (*this - steady_clock::now())).time_since_epoch()).count();
    }
};
