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
#include <memory>
#include "wrt/optional.hpp"
#include <type_traits>
#include <vector>

class RTCConnection;
namespace rtc_state {
using namespace std::chrono;
class VerificationSchedule;
using wrt::optional;

class Offered {
    struct elem_t {
        uint64_t conId;
        bool used = false;
    };
    struct ret_t {
        wrt::optional<uint64_t> conId;
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
    size_t available() const { return allowed - used; }
    [[nodiscard]] auto take_one()
    {
        if (available() < 1) {
            used = allowed;
            throw Error(ERTCQUOTA_FO);
        }
        return used += 1;
    }

    void increase_allowed(uint8_t add)
    {
        allowed += add;
    }

private:
    size_t used { 0 };
    size_t allowed { 0 };
};

class SignalingLists {
public:
    void set(const std::vector<std::pair<uint64_t, IP>>& conIds)
    {
        offsetScheduled = offset + entries.size();
        entries.reserve(entries.size() + conIds.size());
        for (auto& [conId, ip] : conIds) {
            entries.push_back({ conId, ip });
        }
    }

    [[nodiscard]] wrt::optional<uint64_t> get_con_id(uint64_t key)
    {
        uint64_t i { key - offset };
        if (i >= entries.size())
            throw Error(EINV_RTCOFFER);
        auto& e { entries[i] };
        if (e.used)
            throw Error(EDUP_RTCOFFER);
        e.used = true;

        if (key < offsetScheduled)
            return {};

        return e.conId;
    }

    void discard_up_to(uint64_t offsetEnd)
    {
        size_t n(offsetEnd - offset);
        assert(n <= entries.size());
        entries.erase(entries.begin(), entries.begin() + n);
        offset = offsetEnd;
        assert(offsetScheduled - offset <= entries.size());
    }

    auto offset_scheduled() const { return offsetScheduled; }

private:
    struct Entry {
        Entry(uint64_t conId, IP ip)
            : conId(conId)
            , ip(std::move(ip))
        {
        }
        uint64_t conId;
        IP ip;
        bool used { false };
    };
    uint64_t offset { 0 };
    uint64_t offsetScheduled { 0 };
    std::vector<Entry> entries;
};

class PendingForwards {
public:
    struct Entry {
        IP ip;
        uint32_t fromKey;
        uint64_t fromConId;
    };
    struct VecEntry {
        VecEntry(Entry e)
            : entry(std::move(e))
            , insertedAt(steady_clock::now())
        {
        }
        Entry entry;
        steady_clock::time_point insertedAt;
        bool used { false };
    };
    void add(IP ip, uint32_t fromKey, uint64_t fromConId)
    {
        entries.push_back(Entry { ip, fromKey, fromConId });
    }
    [[nodiscard]] Entry get(size_t key)
    {
        auto index { key - offset };
        if (index >= entries.size())
            throw Error(ERTCINV_RFA);
        auto& ve { entries[index] };
        if (ve.used)
            throw Error(ERTCDUP_RFA);
        ve.used = true;
        return std::move(ve.entry);
    }

    template <typename unused_cb_t>
    requires std::is_invocable_v<unused_cb_t, const Entry&>
    optional<steady_clock::time_point> prune(const unused_cb_t& unusedCb)
    {
        optional<steady_clock::time_point> res;
        auto iter = entries.begin();
        for (;; ++iter) {
            if (iter == entries.end())
                break;
            if (iter->used == false) {
                unusedCb(iter->entry);
                res = iter->insertedAt;
                break;
            }
        }
        entries.erase(entries.begin(), iter);
        return res;
    }

    size_t end_offset() const { return offset + entries.size(); }

private:
    size_t offset { 0 };
    std::vector<VecEntry> entries;
};

struct PendingOutgoing {
    using webrtc_con_t = std::weak_ptr<RTCConnection>;
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
    void insert(webrtc_con_t con)
    {
        entries.push_back({ std::move(con) });
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
    [[nodiscard]] const webrtc_con_t& get_rtc_con(uint32_t key)
    {
        uint32_t i { key - offset };
        if (i >= entries.size())
            throw Error(ERTCINV_FA);
        auto& e { entries[i] };
        if (e.used)
            throw Error(ERTCDUP_FA);
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
            throw Error(ERTCDISCARD_FA);
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
    [[nodiscard]] auto set_new_list_size(uint64_t newSize)
    {
        indexOffset += size;
        size = newSize;
        return indexOffset;
    }

    [[nodiscard]] bool covers(uint64_t index) const
    {
        return (index - indexOffset) < size;
    }

private:
    uint64_t indexOffset { 0 };
    uint64_t size { 0 };
};

struct Identity {
public:
    template <typename ip_type>
    struct IpEntry {
        ip_type ip;
        bool verified { false };
        bool verificationTried { false };
        IpEntry(ip_type ip)
            : ip(std::move(ip))
        {
        }
    };
    bool empty() const { return fresh; }
    bool contains(IP ip) const
    {
        return (ipv4.has_value() && ipv4->ip == ip)
            || (ipv6.has_value() && ipv6->ip == ip);
    }
    void set(IdentityIps id)
    {
        if (!fresh)
            throw Error(ERTCDUP_ID);
        fresh = false;
        if (id.get_ip4())
            ipv4 = IpEntry<IPv4>(*id.get_ip4());
        if (id.get_ip6())
            ipv6 = IpEntry<IPv6>(*id.get_ip6());
    }

    [[nodiscard]] wrt::optional<IP> pop_unverified(IdentityIps::Pattern p)
    {
        if (p.ipv4 && ipv4 && ipv4->verificationTried == false) {
            ipv4->verificationTried = true;
            return ipv4->ip;
        }
        if (p.ipv6 && ipv6 && ipv6->verificationTried == false) {
            ipv6->verificationTried = true;
            return ipv6->ip;
        }
        return {};
    }
    void set_verified(IP ip)
    {
        if (ipv4->ip == ip)
            ipv4->verified = true;
        else if (ipv6->ip == ip)
            ipv6->verified = true;
    }

    bool ip_is_verified(IP ip) const
    {
        if (ipv4->ip == ip)
            return ipv4->verified;
        if (ipv6->ip == ip)
            return ipv6->verified;
        return false;
    }

    const IPv4* verified_ip4() const
    {
        return (ipv4 && ipv4->verified ? &ipv4->ip : nullptr);
    }
    const IPv6* verified_ip6() const
    {
        return (ipv6 && ipv6->verified ? &ipv6->ip : nullptr);
    }

private:
    wrt::optional<IpEntry<IPv4>> ipv4;
    wrt::optional<IpEntry<IPv6>> ipv6;
    bool fresh { true };
};

class PendingVerification {
public:
    struct Entry {
        std::shared_ptr<RTCConnection> con;
    };

    void start(std::shared_ptr<RTCConnection> con)
    {
        assert(entry.has_value() == false);
        entry = { std::move(con) };
    }
    void done()
    {
        entry.reset();
    }
    bool has_value() const { return entry.has_value(); }
    auto& value() { return entry.value(); }

private:
    wrt::optional<Entry> entry;
};

class VerificationScheduleData {
    friend class VerificationSchedule;

private:
    bool verificationScheduled { false };
};

struct PeerRTCState : public VerificationScheduleData {
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
        PendingVerification pendingVerification;
        SignalingLists signalingList;
    } our;
    bool enanabled{false};
};
}

using PeerRTCState = rtc_state::PeerRTCState;
