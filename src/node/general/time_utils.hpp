#pragma once
#include <chrono>

namespace timing {

using namespace std::chrono;
using sc = std::chrono::steady_clock;

struct Duration : public sc::duration {
private:
    template<typename duration_t> auto count()const{
        return std::chrono::duration_cast<duration_t>(*this).count();
    }
public:
    std::string format();
    auto seconds() const { return count<std::chrono::seconds>(); }
    auto milliseconds() const { return count<std::chrono::milliseconds>();}
};

struct Tic {
    sc::time_point begin;
    Tic()
        : begin(sc::now())
    {
    }

    Duration elapsed() { return { sc::now() - begin }; }
};
}
