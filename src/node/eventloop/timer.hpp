#pragma once
#include "types/peer_requests.hpp"
#include <chrono>
#include <map>
#include <variant>
#include <vector>

class Timer {

public:
    struct WithConnecitonId {
        uint64_t conId;
    };
    struct SendPing: public WithConnecitonId {
    };
    struct ThrottledProcessMsg: public WithConnecitonId {
    };
    struct Expire: public WithConnecitonId {
    };
    struct Req {
        Request req;
    };
    struct CloseNoReply: public WithConnecitonId {
    };
    struct CloseNoPong: public WithConnecitonId {
    };
    struct Connect {
    };
    using Event = std::variant<SendPing, ThrottledProcessMsg, Expire, CloseNoReply,CloseNoPong, Connect>;

private:
    using time_point = std::chrono::steady_clock::time_point;
    using Ordered = std::multimap<time_point, Event>;

public:
    using iterator = Ordered::iterator;
    // Methods

    template <typename _Rep, typename _Period>
    auto insert(std::chrono::duration<_Rep,_Period> duration, Event e){
        auto expires = std::chrono::steady_clock::now() + duration;
        return insert(expires,e);
    }
    auto insert(std::chrono::steady_clock::time_point expires, Event e){
        return ordered.emplace(expires,e);
    }
    void cancel(Timer::iterator iter)
    {
        if (iter != ordered.end())
            ordered.erase(iter);
    }
    iterator end() { return ordered.end(); }
    std::vector<Event> pop_expired();
    time_point next();

private:
    Ordered ordered;
};

