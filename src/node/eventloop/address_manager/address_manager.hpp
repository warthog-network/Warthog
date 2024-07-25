#pragma once
#ifndef DISABLE_LIBUV
#include "tcp_connections.hpp"
#else
#include "websocket_outbound_schedule.hpp"
#endif

#include "init_arg.hpp"
#include "../types/conndata.hpp"
#include "expected.hpp"
#include "transport/helpers/per_ip_counter.hpp"
#include <chrono>
#include <map>
#include <set>
#include <vector>
class Conref;
struct Inspector;

class AddressManager {
    using time_point = std::chrono::steady_clock::time_point;

public:
    friend struct ::Inspector;
    using InitArg = address_manager::InitArg;
    struct OutboundClosedEvent {
        OutboundClosedEvent( std::shared_ptr<ConnectionBase> c, int32_t reason);
        std::shared_ptr<ConnectionBase> c;
        int32_t reason;
    };
private:
    class ConrefIter : public Coniter {
    public:
        Conref operator*() { return Conref { *this }; }
    };
    struct AllRange {
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
    struct InitializedRange {
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
    AddressManager(InitArg);
    void start();

    // for range-based access
    InitializedRange initialized() const { return { *this }; }
    AllRange all() const { return { *this }; }

    void outbound_failed(const ConnectRequest& r);
    void outbound_closed(OutboundClosedEvent);

    void verify(std::vector<TCPPeeraddr>, IPv4 source); // TODO call this function
    [[nodiscard]] std::optional<Conref> find(uint64_t id) const;
    size_t size() const { return conndatamap.size(); }
    size_t ip_count(const IP& ip) const { return ipCounter.count(ip); };
    bool erase(Conref); // returns whether is pinned

    struct InsertData {
        ConnectionBase::ConnectionVariant& convar;
        HeaderDownload::Downloader& headerDownload;
        BlockDownload::Downloader& blockDownload;
        Timer& timer;
        std::function<void(Conref evictionCandidate)> evict_cb;
    };
    auto insert(InsertData) -> tl::expected<Conref, int32_t>;

#ifndef DISABLE_LIBUV
    auto sample_verified_tcp(size_t N)
    {
        return tcpConnectionSchedule.sample_verified(N);
    }
#endif

    void garbage_collect();

    void start_scheduled_connections();
    [[nodiscard]] std::optional<time_point> pop_scheduled_connect_time();

private:
    bool is_own_endpoint(Peeraddr a);
    std::optional<Conref> eviction_candidate() const;

#ifndef DISABLE_LIBUV
#endif

private:
    std::vector<Peeraddr> outboundEndpoints;
    std::vector<Conref> inboundConnections;
#ifndef DISABLE_LIBUV
    TCPConnectionSchedule tcpConnectionSchedule;
#else 
    WSConnectionSchedule wsConnectionSchedule;
#endif

    mutable Conndatamap conndatamap;
    std::vector<Conref> delayedDelete;
    PerIpCounter ipCounter;
};
