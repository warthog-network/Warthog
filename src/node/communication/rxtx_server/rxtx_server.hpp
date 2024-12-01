#pragma once
#include "api/callbacks.hpp"
#include "rxtx_db.hpp"
#include "transport/helpers/ip.hpp"
#include "transport/helpers/peer_addr.hpp"
#include <chrono>
#include <condition_variable>
#include <thread>
#include <variant>

namespace rxtx {

class Server {
private:
    struct Shutdown {
    };

    struct Rx : public TransmissionEvent {
        using TransmissionEvent::TransmissionEvent;
    };
    struct Tx : public TransmissionEvent {
        using TransmissionEvent::TransmissionEvent;
    };

    struct GetAggregate {
        TransmissionCb cb;
        TimestampRange range;
    };

    struct GetAggregateHours : public GetAggregate {
        GetAggregateHours(GetAggregate a)
            : GetAggregate(std::move(a)) {};
    };
    struct GetAggregateMinutes : public GetAggregate {
        GetAggregateMinutes(GetAggregate a)
            : GetAggregate(std::move(a)) {};
    };

    using event_t = std::variant<Shutdown, Rx, Tx, GetAggregateHours, GetAggregateMinutes>;
    using events_t = std::vector<event_t>;
    void push_event(event_t event)
    {
        std::lock_guard l(m);
        events.push_back(std::move(event));
        cv.notify_all();
    }
    void run();
    void process_events(events_t&&);
    void finalize_aggregators(Timestamp t, bool drain);
    bool has_events();

    void handle_event(Shutdown&&);
    void handle_event(Rx&&);
    void handle_event(Tx&&);
    void handle_event(GetAggregateHours&&);
    void handle_event(GetAggregateMinutes&&);

public:
    ~Server();
    void start();
    void shutdown();
    void add_rx(const Peerhost&, size_t nBytes);
    void add_tx(const Peerhost&, size_t nBytes);
    void api_get_aggregate_minutes(GetAggregate);
    void api_get_aggregate_hours(GetAggregate);

private:
    // internal variables
    Buckets<60> aggregatorMinute;
    Buckets<60 * 60> aggregatorHour;
    DB db;

    // threading variables
    bool needs_shutdown { false };
    std::condition_variable cv;
    std::mutex m;
    events_t events;
    std::thread t;
};
}
