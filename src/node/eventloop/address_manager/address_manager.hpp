#pragma once
#include "../types/conndata.hpp"
#include "connection_schedule.hpp"
#include "expected.hpp"
#include "transport/helpers/per_ip_counter.hpp"
#include <chrono>
#include <map>
#include <set>
#include <vector>
class Conref;
class TCPConnection;
struct Inspector;


class AddressManager {
    using time_point = std::chrono::steady_clock::time_point;

public:
    friend struct ::Inspector;
    using InitArg = connection_schedule::InitArg;

private:
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
    AddressManager(connection_schedule::InitArg);
    void start();

    // for range-based access
    Initialized initialized() const { return { *this }; }
    All all() const { return { *this }; }

    void outbound_failed(const ConnectRequest& r);
    void verify(std::vector<TCPSockaddr>, IPv4 source); // TODO call this function

    // access by connection Id
    [[nodiscard]] std::optional<Conref> find(uint64_t id) const;

    size_t size() const { return conndatamap.size(); }

    size_t ip_count(const IP& ip) const { return ipCounter.count(ip); };
    // erase/insert
    bool erase(Conref); // returns whether is pinned
    // [[nodiscard]] tl::expected<std::optional<Conref>, int32_t> prepare_insert(const ConnectionBase::ConnectionVariant&);
    // Conref insert_prepared(const std::shared_ptr<ConnectionBase>&, HeaderDownload::Downloader&, BlockDownload::Downloader&, Timer&);

    struct InsertData {
        ConnectionBase::ConnectionVariant& convar;
        HeaderDownload::Downloader& headerDownload;
        BlockDownload::Downloader& blockDownload;
        Timer& timer;
        std::function<void(Conref evictionCandidate)> evict_cb;
    };
    auto insert(InsertData) -> tl::expected<Conref, int32_t>;

    // access queued

    // template <typename T>
    // [[nodiscard]] std::vector<T> sample_verified(size_t N)
    // {
    //     return connectionSchedule.sample_verified<T>(N);
    // }
    auto sample_verified(size_t N)
    {
        return connectionSchedule.sample_verified(N);
    }

    void garbage_collect();

    void start_scheduled_connections();
    [[nodiscard]] std::optional<time_point> pop_scheduled_connect_time();

private:
    // [[nodiscard]] tl::expected<std::optional<Conref>, int32_t> prepare_insert(const std::shared_ptr<TCPConnection>&);
    // [[nodiscard]] tl::expected<std::optional<Conref>, int32_t> prepare_insert(const std::shared_ptr<WSConnection>&);
    // [[nodiscard]] tl::expected<std::optional<Conref>, int32_t> prepare_insert(const std::shared_ptr<RTCConnection>&);
    bool is_own_endpoint(Sockaddr a);
    std::optional<Conref> eviction_candidate() const;

private:
    // data

    std::vector<Sockaddr> outboundEndpoints;
    std::vector<Conref> inboundConnections;
    ConnectionSchedule connectionSchedule;

    mutable Conndatamap conndatamap;
    std::vector<Conref> delayedDelete;
    PerIpCounter ipCounter;
};
