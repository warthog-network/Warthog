#pragma once
#include "general/move_only_function.hpp"
#include <chrono>
#include <map>
#include <variant>

class Timer {
    using time_point = std::chrono::steady_clock::time_point;

public:
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

    struct CallFunction {
        MoveOnlyFunction<void()> callback;
    };
    using Event = std::variant<SendPing, Expire, CloseNoReply, CloseNoPong, ScheduledConnect, CallFunction>;

private:
public:
    struct key_t {
        time_point wakeup_tp;
        int i;
        auto operator<=>(const key_t&) const = default;
    };
    using Ordered = std::map<key_t, Event>;
    using iterator = Ordered::iterator;
    // Methods

    auto insert(time_point expires, Event e)
    {
        key_t key { expires, keyExtra++ };
        auto [iter, _inserted] { ordered.emplace(key, std::move(e)) };
        return iter;
    }
    auto insert(std::chrono::steady_clock::duration duration, Event e)
    {
        time_point expires { std::chrono::steady_clock::now() + duration };
        return insert(expires, std::move(e));
    }
    void cancel(Timer::iterator iter)
    {
        if (iter != ordered.end())
            ordered.erase(iter);
    }
    void cancel(key_t key)
    {
        ordered.erase(key);
    }
    iterator end() { return ordered.end(); }
    std::vector<Event> pop_expired();
    time_point next();

private:
    Ordered ordered;
    int keyExtra { 0 };
};
