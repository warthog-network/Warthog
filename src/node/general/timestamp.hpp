#pragma once

#include <chrono>
#include <optional>
// #include <compare>
// #include <cstdint>
template <uint32_t seconds>
class RoundedTimestamp;
class Timestamp {

public:
    Timestamp(uint32_t v)
        : data(v)
    {
    }
    auto operator<=>(const Timestamp&) const = default;
    static Timestamp now()
    {
        using namespace std::chrono;
        return Timestamp(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
    }
    static Timestamp from_time_point(std::chrono::steady_clock::time_point tp)
    {
        using namespace std::chrono;
        return duration_cast<seconds>((system_clock::now() + (tp - steady_clock::now())).time_since_epoch()).count();
    }
    template <uint32_t seconds>
    RoundedTimestamp<seconds> floor() const;

    template <uint32_t seconds>
    RoundedTimestamp<seconds> ceil() const;

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

struct TimestampRange{
    Timestamp begin;
    Timestamp end;
};

template <uint32_t seconds>
class RoundedTimestamp : public Timestamp {
private:
    using Timestamp::Timestamp;
    friend class Timestamp;

public:
    static std::optional<RoundedTimestamp<seconds>> try_from_timestamp(Timestamp ts)
    {
        if (ts.val() % seconds == 0)
            return RoundedTimestamp<seconds> { ts };
        return {};
    }
    static RoundedTimestamp zero()
    {
        return { 0 };
    }
    RoundedTimestamp prev() const
    {
        return { val() - seconds};
    }
};

template <uint32_t seconds>
RoundedTimestamp<seconds> Timestamp::floor() const
{
    return { (data / seconds) * seconds };
}
template <uint32_t seconds>
RoundedTimestamp<seconds> Timestamp::ceil() const
{
    return { ((data + seconds - 1) / seconds) * seconds };
}

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
