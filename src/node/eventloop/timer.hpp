#pragma once
#include "general/move_only_function.hpp"
#include <chrono>
#include <map>
#include <variant>

namespace eventloop {
namespace timer_events {
    struct WithConnecitonId {
        uint64_t conId;
    };
    struct SendPing : public WithConnecitonId {
    };
    struct Expire : public WithConnecitonId {
    };
    struct CloseNoReply : public WithConnecitonId {
    };
    struct CloseNoPong : public WithConnecitonId {
    };
    struct ScheduledConnect {
    };
    struct SendIdentityIps {
    };
    struct CallFunction {
        MoveOnlyFunction<void()> callback;
    };
    using Event = std::variant<SendPing, Expire, CloseNoReply, CloseNoPong, ScheduledConnect, SendIdentityIps, CallFunction>;
}

class Timer;
class TimerSystem {
    using time_point = std::chrono::steady_clock::time_point;

public:
    TimerSystem() {};
    TimerSystem(const TimerSystem&) = delete;

    using Event = timer_events::Event;

public:
    struct key_t {
        time_point wakeup_tp;
        int i;
        auto operator<=>(const key_t&) const = default;
    };
    using Ordered = std::map<key_t, Event>;
    using const_iterator = Ordered::const_iterator;
    // Methods

    Timer disabled_timer() const;

    bool is_disabled(const Timer&) const;
    Timer insert(time_point expires, Event e);
    Timer insert(std::chrono::steady_clock::duration duration, Event e);
    void cancel(Timer t);
    void cancel(key_t key)
    {
        ordered.erase(key);
    }
    const_iterator end() { return ordered.end(); }
    std::vector<Event> pop_expired();
    time_point next();

private:
    Ordered ordered;
    int keyExtra { 0 };
};

class Timer {
    friend class TimerSystem;
    Timer(TimerSystem::const_iterator iter)
        : timer_iter(iter)
    {
    }

public:
    TimerSystem::const_iterator& timer_ref() { return timer_iter; }
    void reset_expired(TimerSystem&);
    void reset_notexpired(TimerSystem&);
    TimerSystem::key_t key() const
    {
        return timer_iter->first;
    }
    auto wakeup_tp() const
    {
        return timer_iter->first.wakeup_tp;
    }

    TimerSystem::const_iterator timer() { return timer_iter; }

protected:
    TimerSystem::const_iterator timer_iter;
};

inline Timer TimerSystem::disabled_timer() const
{
    return { ordered.end() };
}

inline bool TimerSystem::is_disabled(const Timer& t) const
{
    return t.timer_iter == ordered.end();
}

inline void TimerSystem::cancel(Timer t)
{
    if (is_disabled(t))
        return;
    ordered.erase(t.timer_iter);
    t = disabled_timer();
}

inline Timer TimerSystem::insert(std::chrono::steady_clock::duration duration, Event e)
{
    time_point expires { std::chrono::steady_clock::now() + duration };
    return insert(expires, std::move(e));
}
inline Timer TimerSystem::insert(time_point expires, Event e)
{
    key_t key { expires, keyExtra++ };
    auto [iter, _inserted] { ordered.emplace(key, std::move(e)) };
    return { iter };
}

}
