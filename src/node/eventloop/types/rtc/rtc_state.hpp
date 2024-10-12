#pragma once
#include "eventloop/types/conref_declaration.hpp"
#include "registry_type.hpp"
#include "transport/helpers/ip.hpp"
#include "transport/webrtc/rtc_connection.hpp"
#include "transport/webrtc/sdp_util.hpp"
#include <list>
#include <optional>

class RTCConnection;
namespace rtc_state {

class VerificationSchedule {
    struct PopResult {
        Conref conref;
        IP ip;
    };

public:
    void add(Conref c);
    void erase(Conref c);
    bool empty() const { return offset >= queue.size(); }
    std::optional<PopResult> pop(IdentityIps::Pattern);

private:
    std::optional<Conref> pop_front();
    std::vector<Conref> queue;
    size_t offset = 0;
    static constexpr size_t pruneOffset = 10;
};

class Connections {
public:
    void insert(std::shared_ptr<RTCConnection>);
    void erase(std::shared_ptr<RTCConnection>&);
    auto size() const { return entries.size(); }
    bool can_insert_standard() const { return size() < maxStandardEntries; }
    bool can_insert_feeler() const { return size() < maxFeelerEntries; }

private:
    registry_t entries;
    size_t maxStandardEntries { 80 };
    size_t maxFeelerEntries { 100 };
};

struct RTCState {

    [[nodiscard]] std::optional<IP> get_ip(IpType t) const;
    void erase(Conref);
    size_t total { 0 };
    // size_t hardMax { 100 };
    // size_t
    std::optional<IdentityIps> ips;
    Connections connections;
    VerificationSchedule verificationSchedule;
};
}
using RTCState = rtc_state::RTCState;
