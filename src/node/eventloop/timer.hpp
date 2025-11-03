#pragma once
#include "general/move_only_function.hpp"
#include <chrono>
#include <map>
#include "wrt/optional.hpp"
#include <variant>

namespace eventloop {
namespace timer_events {
    struct WithConnecitonId {
        uint64_t conId;
    };
    struct SendPing : public WithConnecitonId {
    };
    struct ThrottledProcessMsg: public WithConnecitonId {
    };
    struct Expire: public WithConnecitonId {
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
    using Event = std::variant<SendPing, ThrottledProcessMsg, Expire, CloseNoReply, CloseNoPong, ScheduledConnect, SendIdentityIps, CallFunction>;
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

    bool erase(const Timer&);
    bool erase(const wrt::optional<Timer>& o);
    bool erase(key_t k)
    {
        return ordered.erase(k) != 0;
    }

    Timer insert(time_point expires, Event e);
    Timer insert(std::chrono::steady_clock::duration duration, Event e);
    const_iterator end() { return ordered.end(); }
    std::vector<Event> pop_expired();
    time_point next();

private:
    Ordered ordered;
    int keyExtra { 0 };
};

class Timer {
    friend class TimerSystem;
    Timer(TimerSystem::key_t k)
        : _key(k)
    {
    }

public:
    // TimerSystem::const_iterator& timer_ref() { return timer_iter; }
    TimerSystem::key_t key() const
    {
        return _key;
    }
    auto wakeup_tp() const
    {
        return _key.wakeup_tp;
    }

    // TimerSystem::const_iterator timer() { return timer_iter; }

protected:
    TimerSystem::key_t _key;
};

inline Timer TimerSystem::insert(std::chrono::steady_clock::duration duration, Event e)
{
    time_point expires { std::chrono::steady_clock::now() + duration };
    return insert(expires, std::move(e));
}

inline Timer TimerSystem::insert(time_point expires, Event e)
{
    key_t key { expires, keyExtra++ };
    ordered.emplace(key, std::move(e));
    return { key };
}

inline bool TimerSystem::erase(const Timer& t)
{
    return ordered.erase(t.key());
}
inline bool TimerSystem::erase(const wrt::optional<Timer>& o)
{
    return o && erase(*o);
}

}
