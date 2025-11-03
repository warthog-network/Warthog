#pragma once
#include "timer.hpp"
#include "wrt/optional.hpp"

class TimerElement {
    using key_t = eventloop::TimerSystem::key_t;
public:
    TimerElement() {};
    TimerElement(key_t key)
        : key(key)
    {
    }
    TimerElement& operator=(TimerElement&& other)
    {
        key = std::move(other.key);
        other.key.reset();
        return *this;
    }

    TimerElement(TimerElement&& other)
    {
        *this = std::move(other);
    }
    void reset() { key.reset(); }
    ~TimerElement();

private:
    wrt::optional<key_t> key;
};
