#pragma once
#include "../types/conndata.hpp"
#include "flat_address_set.hpp"
#include "general/tcp_util.hpp"
#include <chrono>
#include <map>
#include <set>
#include <vector>
struct PeerServer;
class Conref;
class Connection;
struct Inspector;

namespace address_manager {

struct AddressManager;

struct AddressManager {
    friend struct ::Inspector;

private:
    struct VerifiedState;
    struct PinState;

    using sc = std::chrono::steady_clock;
    using VerifiedMap = std::map<EndpointAddress, VerifiedState>;
    using VerIter = VerifiedMap::iterator;
    using PinnedMap = std::map<EndpointAddress, PinState>;
    using PinIter = PinnedMap::iterator;
    using PendingMap = std::map<EndpointAddress, sc::time_point>;
    using TimerVal = std::variant<VerIter, PinIter>;
    using TimerType = std::multimap<sc::time_point, TimerVal>;

    //////////////////////////////
    // struct definitions
    struct VerifiedState {
        VerifiedState(TimerType::iterator end)
            : timer_iter(end)
        {
        }
        TimerType::iterator timer_iter;
        std::chrono::steady_clock::time_point lastVerified;
        bool outboundConnection = false;
    };
    struct PinState {
        PinState()
            : rateLimit(sc::now())
        {
        }
        static constexpr auto bucket_weight = std::chrono::seconds(10);
        static constexpr auto bucket_size = 2 * std::chrono::seconds(10);
        size_t sleepSeconds = 0;
        std::chrono::steady_clock::time_point rateLimit;
        std::chrono::steady_clock::time_point ratelimit_sleep()
        {
            auto now = sc::now();
            auto a = now + bucket_size;
            if (rateLimit < now) {
                rateLimit = now;
            }
            rateLimit += bucket_weight;
            if (rateLimit < a)
                return now;
            return now + (rateLimit - a);
        }
        TimerType::iterator timer_iter;
    };
    class ConrefIter : public Coniter {
    public:
        Conref operator*() { return Conref { *this }; }
    };
    struct All {
        const AddressManager& ref;
        struct End {
        };
        struct Iterator {
            ConrefIter iter;
            const AddressManager& ref;
            Iterator(ConrefIter iter, const AddressManager& ref)
                : iter(iter)
                , ref(ref)
            {
                find_next();
            }
            Iterator& operator++()
            {
                ++iter;
                find_next();
                return *this;
            }
            Conref operator*() { return iter; }
            bool operator==(End)
            {
                return iter == ref.conndatamap.end();
            }

        private:
            void find_next();
        };
        auto begin() const { return Iterator {
            { ref.conndatamap.begin() },
            ref
        }; }
        auto end() const { return End {}; }
        bool empty() { return begin() == end(); };
    };
    struct Initialized {
        const AddressManager& ref;
        struct End {
        };
        struct Iterator {
            ConrefIter iter;
            const AddressManager& ref;
            Iterator(ConrefIter iter, const AddressManager& ref)
                : iter(iter)
                , ref(ref)
            {
                find_next();
            }
            Iterator& operator++()
            {
                ++iter;
                find_next();
                return *this;
            }
            Conref operator*() { return iter; }
            bool operator==(End)
            {
                return iter == ref.conndatamap.end();
            }

        private:
            void find_next();
        };
        auto begin() const { return Iterator {
            { ref.conndatamap.begin() },
            ref
        }; }
        auto end() const { return End {}; }
        bool empty() { return begin() == end(); };
    };

public:
    //////////////////////////////
    // public methods

    // constructor
    AddressManager(PeerServer& peerServer, const std::vector<EndpointAddress>& v);

    // for range-based access
    Initialized initialized() const { return { *this }; }
    All all() const { return { *this }; }

    // access by connection Id
    Conref find(uint64_t id);

    size_t size() const { return conndatamap.size(); }

    // erase/insert
    [[nodiscard]] bool erase(Conndatamap::iterator); // returns whether is pinned
    std::pair<int32_t, Conref> insert(std::shared_ptr<Connection>,
        HeaderDownload::Downloader&, BlockDownload::Downloader&, Timer&);

    // callbacks
    [[nodiscard]] bool on_failed_outbound(EndpointAddress); // returns whether is pinned

    // access queued
    std::vector<EndpointAddress> pop_connect();
    void queue_verification(const std::vector<EndpointAddress>&);

    // pin control
    std::optional<std::chrono::steady_clock::time_point> wakeup_time();
    bool unpin(EndpointAddress);
    bool pin(EndpointAddress);
    void pin(const std::vector<EndpointAddress>&);

    [[nodiscard]] std::vector<EndpointAddress> sample_verified(size_t N);
    void garbage_collect();

private:
    void queue_verification(EndpointAddress);
    void check_prune_verified();
    void just_verified(EndpointAddress, bool setTimer);
    void remove_timer(VerIter);
    void set_timer(sc::time_point, VerIter);
    void remove_timer(PinIter);
    void insert_unverified(EndpointAddress a);
    bool is_own_endpoint(EndpointAddress a);

private:
    // data
    PeerServer& peerServer;
    size_t maxPending = 20;
    size_t maxRecent = 100;
    size_t verifiedPruneAt = 200;
    size_t verifiedPruneTo = 100;
    const std::vector<IPv4> ownIps;

    FlatAddressSet failedAddresses;

    // maps/sets by EndpointAddress
    std::set<EndpointAddress> unverifiedAddresses;
    VerifiedMap verified;
    PinnedMap pinned;

    // verifiedCache as vector for fast sampling
    std::vector<EndpointAddress> verifiedCache;
    std::optional<sc::time_point> cacheExpire;

    // Timer
    TimerType timer;
    PendingMap pendingOutgoing;
    mutable Conndatamap conndatamap;
    std::vector<Conndatamap::iterator> delayedDelete;
    std::map<EndpointAddress, Coniter> byEndpoint;
};
}
