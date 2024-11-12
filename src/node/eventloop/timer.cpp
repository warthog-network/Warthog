#include "timer.hpp"
#include <cassert>

namespace eventloop {
std::vector<TimerSystem::Event> TimerSystem::pop_expired()
{
    const auto now = std::chrono::steady_clock::now();
    std::vector<TimerSystem::Event> res;
    for (auto iter = ordered.begin(); iter != ordered.end();) {
        if (iter->first.wakeup_tp > now)
            break;
        res.push_back(std::move(iter->second));
        ordered.erase(iter++);
    }
    return res;
}

TimerSystem::time_point TimerSystem::next()
{
    if (ordered.empty()) {
        return std::chrono::steady_clock::now() + std::chrono::days(1);
        // this does not work on docker alpine 3.15 (wait_until fires immediately)
        // return time_point::max();
    }
    return ordered.begin()->first.wakeup_tp;
};

void Timer::reset_expired(TimerSystem& ts)
{
    assert(!ts.is_disabled(*this));
    *this = ts.disabled_timer();
}
void Timer::reset_notexpired(TimerSystem& ts)
{
    assert(!ts.is_disabled(*this));
    ts.cancel(timer_iter);
    *this = ts.disabled_timer();
}
}
