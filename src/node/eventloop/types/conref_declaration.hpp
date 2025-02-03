#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <chrono>
struct PeerState;
class PeerChain;
class Connection;
class Sndbuffer;
using Conndatamap = std::map<uint64_t, PeerState>;
using Coniter = Conndatamap::iterator;

class Conref {
    union Data {
        uint64_t val = 0;
        Coniter iter;
    };
    using duration = std::chrono::steady_clock::duration;

public:
    inline bool operator<(Conref other) const { return data.val < other.data.val; }
    inline bool operator==(Conref other) const;
    inline operator Connection*();
    inline operator const Connection*() const;
    inline const PeerChain& chain() const;
    inline const PeerState& state() const;
    inline PeerState& state();
    inline PeerChain& chain();
    operator bool() { return data.val != 0; };
    inline bool closed();
    inline auto& job();
    inline auto& job() const;
    inline auto& ping();
    inline auto operator->();
    void clear() { data.val = 0; }
    inline bool initialized();
    void send(Sndbuffer);
    Conref()
        : data({ .val = 0ul })
    {
    }
    Conref(Coniter iter)
        : data({ .iter = iter })
    {
    }
    Coniter iterator() { return data.iter; };
    bool valid() const { return data.val != 0ul; };
    uint64_t id() const;
    std::string str() const;

    template <typename... Args>
    consteval inline void debug(auto fmt, Args&&... args);

    template <typename... Args>
    inline void info(const char* fmt, Args&&... args);

    template <typename... Args>
    inline void warn(const char* fmt, Args&&... args);

private:
    Data data;
    static_assert(sizeof(Data) == sizeof(uint64_t));
};
