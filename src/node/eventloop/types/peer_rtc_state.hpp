#pragma once
#include "api/callbacks.hpp"
#include "general/errors.hpp"

#include "transport/helpers/ip.hpp"
#include "transport/webrtc/sdp_util.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <vector>

namespace rtc_state {
using namespace std::chrono;

class Offered {
    struct elem_t {
        uint64_t conId;
        bool used = false;
    };
    struct ret_t {
        std::optional<uint64_t> conId;
        bool repeated { false };
    };
    [[nodiscard]] uint32_t begin() const { return offset; }
    [[nodiscard]] uint32_t end() const { return offset; }

public:
    [[nodiscard]] ret_t use(uint32_t key)
    {
        size_t i { key - offset };
        ret_t ret;
        if (i < data.size()) {
            if (data[i].used) {
                ret.repeated = true;
            } else {
                ret.conId = data[i].conId;
                data[i].used = true;
            }
        }
        return ret;
    }

private:
    std::deque<elem_t> data;
    uint32_t offset { 0 };
};

class Quota {
public:
    uint64_t available() const { return allowed - used; }
    [[nodiscard]] bool take_one()
    {
        if (available() < 1) {
            used = allowed;
            return false;
        }
        used += 1;
        return true;
    }

    void increase_allowed(uint8_t add)
    {
        allowed += add;
    }

private:
    uint64_t used { 0 };
    uint64_t allowed { 0 };
};

class SignalingLists {
public:
    [[nodiscard]] auto set(const std::vector<uint64_t>& conIds)
    {
        offsetScheduled = offset + entries.size();
        entries.reserve(entries.size() + conIds.size());
        for (auto& conId : conIds) {
            entries.push_back({ conId });
        }
    }
    [[nodiscard]] std::optional<uint64_t> get_con_id(uint64_t key)
    {
        uint64_t i { key - offset };
        if (i >= entries.size())
            throw Error(ERTCINVFWDRQ);
        auto& e { entries[i] };
        if (e.used)
            throw Error(ERTCDUPFWDRQ);
        e.used = true;

        if (key < offsetScheduled)
            return {};

        return e.conId;
    }

    void discard_up_to(uint64_t offsetEnd)
    {
        size_t n { offsetEnd - offset };
        assert(n <= entries.size());
        entries.erase(entries.begin(), entries.begin() + n);
        offset = offsetEnd;
        assert(offsetScheduled - offset <= entries.size());
    }

    auto offset_scheduled() const { return offsetScheduled; }

private:
    struct Entry {
        Entry(uint64_t conId)
            : conId(conId)
        {
        }

        uint64_t conId;
        bool used { false };
    };
    uint64_t offset { 0 };
    uint64_t offsetScheduled { 0 };
    std::vector<Entry> entries;
};

class PendingForwards {
public:
    struct Entry {
        uint32_t fromKey;
        uint64_t fromConId;
    };
    void add(uint32_t fromKey, uint64_t fromConId)
    {
        entries.push_back({ fromKey, fromConId });
    }
    [[nodiscard]] std::optional<Entry> pop_first()
    {
        if (entries.size() == 0)
            return {};
        Entry res { std::move(entries.front()) };
        offset += 1;
        entries.erase(entries.begin());
        return res;
    }
    bool all_handled(size_t endOffset) const
    {
        return endOffset <= offset;
    }
    size_t end_offset() const { return offset + entries.size(); }

private:
    size_t offset { 0 };
    std::vector<Entry> entries;
};

struct PendingOutgoing {
    using webrtc_con_t = void*;
    struct Entry {
        webrtc_con_t con;
        Entry(webrtc_con_t con)
            : con(std::move(con))
            , inserted(steady_clock::now())
        {
        }
        steady_clock::time_point inserted;
        bool used { false };
        // @SHIFU: add handle to pending WebRTC connection (std::shared_ptr)
    };

public:
    [[nodiscard]] uint32_t register_connection(webrtc_con_t con)
    {
        entries.push_back({ std::move(con) });
        return offset + uint32_t(entries.size() - 1);
    }
    void discard(uint32_t n)
    {
        assert(entries.size() >= n);
        entries.erase(entries.begin(), entries.begin() + n);
        // @SHIFU make sure you delete/shutdown connection handles gracefully
        offset += n;
    }

    bool can_connect(uint32_t maxSize = 20) const
    {
        return size() < maxSize;
    }

    [[nodiscard]] uint32_t schedule_discard(seconds ttl = 60s)
    {
        const auto now { steady_clock::now() };
        const auto discardBefore { now - ttl };
        const size_t i0 = offsetScheduled - offset;
        size_t i = i0;
        for (; i < entries.size(); ++i) {
            if (entries[i].inserted >= discardBefore)
                break;
        }
        uint32_t n = i - i0;
        offsetScheduled += n;
        return n;
    }

    [[nodiscard]] size_t size() const
    {
        return entries.size();
    }
    [[nodiscard]] tl::expected<webrtc_con_t, int32_t> get_rtc_con(uint32_t key)
    {
        uint32_t i { key - offset };
        if (i >= entries.size())
            return tl::make_unexpected(ERTCNOTFOUND);
        auto& e { entries[i] };
        if (e.used)
            return tl::make_unexpected(ERTCDUPLICATE);
        e.used = true;
        return e.con;
    }

private:
    uint32_t scheduled_size() // size after peer confirmed ping message with pong.
                              // In the ping message we transmit how many old
                              // entries to discard
    {
        const uint32_t s1(entries.size());
        const auto s2 { offsetScheduled - offset };
        assert(s1 >= s2);
        return s1 - s2;
    }
    uint32_t offset { 0 };
    uint32_t offsetScheduled { 0 };
    std::vector<Entry> entries;
};

class ForwardRequestState {
public:
    void discard(uint32_t n)
    {
        if (end - begin < n)
            throw Error(ERTCDISCARDFWD);
        begin += n;
    }
    [[nodiscard]] uint32_t create() { return end++; }
    [[nodiscard]] bool is_accepted_key(uint32_t n)
    {
        return (n - begin) < end;
    }

private:
    uint32_t begin { 0 };
    uint32_t end { 0 };
};

class SignalingCounter {
public:
    [[nodiscard]] auto increment_offset(uint64_t v)
    {
        auto tmp { indexOffset };
        indexOffset += v;
        return tmp;
    }

private:
    uint64_t indexOffset { 0 };
};

struct Identity {
public:
    void set(IdentityIps id)
    {
        if (rtcIdentity.has_value())
            throw Error(ERTCDUPLICATEID);
        rtcIdentity = id;
    }
    auto& get() const { return rtcIdentity; }
    std::optional<IPv6> verified_ip6() const
    {
        if (verifiedIpv6 && rtcIdentity)
            return rtcIdentity->get_ip6();
        return std::nullopt;
    }
    
    std::optional<IPv4> verified_ip4() const
    {
        if (verifiedIpv4 && rtcIdentity)
            return rtcIdentity->get_ip4();
        return std::nullopt;
    }


private:
    std::optional<IdentityIps> rtcIdentity;
    bool verifiedIpv4 { false };
    bool verifiedIpv6 { false };
};

struct PeerRTCState {
    struct {
        Quota quota;
        ForwardRequestState forwardRequests;
        SignalingCounter signalingList;
        Identity identity;
    } their;
    struct {
        Quota quota;
        PendingForwards pendingForwards;
        PendingOutgoing pendingOutgoing;
        SignalingLists signalingList;
    } our;
};
}

using PeerRTCState = rtc_state::PeerRTCState;
