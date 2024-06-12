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
class ConnectionBase;

class Conref {
    Coniter iter;

public:
    bool operator==(const Conref&) const;
    [[nodiscard]] const PeerChain& chain() const;
    [[nodiscard]] PeerChain& chain();
    [[nodiscard]] bool closed();
    [[nodiscard]] auto& job();
    [[nodiscard]] auto& job() const;
    [[nodiscard]] auto peer() const;
    [[nodiscard]] auto& rtc();
    [[nodiscard]] auto& ping();
    [[nodiscard]] auto operator->();
    [[nodiscard]] auto version() const;
    [[nodiscard]] bool initialized() const;
    [[nodiscard]] bool is_native() const;
    void send(Sndbuffer);
    Conref(Coniter iter)
        : iter(iter)
    {
    }
    operator const ConnectionBase& ();
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
