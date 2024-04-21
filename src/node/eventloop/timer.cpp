#include "timer.hpp"


std::vector<Timer::Event> Timer::pop_expired() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<Timer::Event> res;
    for (auto iter = ordered.begin(); iter != ordered.end();) {
        if (iter->first.wakeup_tp > now)
            break;
        res.push_back(std::move(iter->second));
        ordered.erase(iter++);
    }
    return res;
}

Timer::time_point Timer::next() {
    if (ordered.empty()){
        return std::chrono::steady_clock::now()+std::chrono::days(1);
        // this does not work on docker alpine 3.15 (wait_until fires immediately)
        // return time_point::max();
    }
    return ordered.begin()->first.wakeup_tp;
};
