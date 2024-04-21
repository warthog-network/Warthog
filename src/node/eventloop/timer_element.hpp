#pragma once
#include "timer.hpp"
#include <optional>

class TimerElement {
public:
    TimerElement() {};
    TimerElement(Timer::key_t key)
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
    std::optional<Timer::key_t> key;
};
