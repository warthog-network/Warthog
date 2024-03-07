#pragma once
#include <cstdint>
#include <map>
#include <string>
class PeerState;
class PeerChain;
class TCPConnection;
class Sndbuffer;
using Conndatamap = std::map<uint64_t, PeerState>;
using Coniter = Conndatamap::iterator;

class Conref {
    Coniter iter;

public:
    bool operator==(const Conref&) const;
    inline const PeerChain& chain() const;
    inline PeerChain& chain();
    inline bool closed();
    inline auto& job();
    inline auto& peer() const;
    inline auto& job() const;
    inline auto& ping();
    inline auto operator->();
    inline bool initialized();
    void send(Sndbuffer);
    Conref(Coniter iter)
        : iter(iter)
    {
    }
    Coniter iterator() { return iter; };
    uint64_t id() const;
    std::string str() const;

    template <typename... Args>
    consteval inline void debug(auto fmt, Args&&... args);

    template <typename... Args>
    inline void info(const char* fmt, Args&&... args);

    template <typename... Args>
    inline void warn(const char* fmt, Args&&... args);
};
