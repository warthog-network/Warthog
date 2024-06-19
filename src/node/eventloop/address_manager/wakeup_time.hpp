#pragma once

#include <chrono>
#include <optional>

namespace connection_schedule {
class WakeupTime {
public:
    using steady_clock = std::chrono::steady_clock;
    using time_point = steady_clock::time_point;
    auto pop()
    {
        auto tmp { std::move(popped_tp) };
        popped_tp.reset();
        return tmp;
    }
    [[nodiscard]] bool expired(time_point now = steady_clock::now()) const
    {
        return wakeup_tp.has_value() && wakeup_tp < now;
    }
    void reset()
    {
        *this = {};
    }

    auto& val() const { return wakeup_tp; }

    void consider(const std::optional<time_point>& newval)
    {
        if (newval.has_value() && (!wakeup_tp.has_value() || *wakeup_tp > *newval)) {
            wakeup_tp = newval;
            popped_tp = newval;
        }
    }

private:
    std::optional<time_point> wakeup_tp;
    std::optional<time_point> popped_tp;
};
}
