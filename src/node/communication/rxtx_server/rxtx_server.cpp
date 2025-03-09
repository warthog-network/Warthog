#include "rxtx_server.hpp"
#include "aggregator.hxx"
#include "api_types.hpp"
#include "spdlog/spdlog.h"

namespace rxtx {

Server::~Server()
{
    shutdown();
    t.join();
}

void Server::shutdown()
{
    push_event(Shutdown {});
}

bool Server::has_events()
{
    std::lock_guard l(m);
    return !events.empty();
}

void Server::api_get_aggregate_minutes(GetAggregate a)
{
    push_event(GetAggregateMinutes(std::move(a)));
}

void Server::api_get_aggregate_hours(GetAggregate a)
{
    push_event(GetAggregateHours(std::move(a)));
}

void Server::start()
{
    t = std::thread([&]() {
        run();
    });
}

void Server::process_events(events_t&& tmp)
{
    for (auto& e : tmp) {
        std::visit([&](auto&& e) {
            handle_event(std::move(e));
        },
            std::move(e));
    }
}
void Server::add_rx(const Peerhost& pa, size_t nBytes)
{
    push_event(Rx(pa, nBytes));
}

void Server::add_tx(const Peerhost& pa, size_t nBytes)
{
    push_event(Tx(pa, nBytes));
}
namespace {
    auto next_timestmamp(std::chrono::steady_clock::duration interval)
    {
        using namespace std::chrono;
        auto n { steady_clock::now() };
        return steady_clock::time_point((n.time_since_epoch() / interval) * interval + interval);
    }

}

void Server::run()
{
    using namespace std::chrono_literals;
    std::chrono::steady_clock::time_point wait_until;
    while (true) {
        events_t tmp;
        {
            std::unique_lock l(m);
            cv.wait_until(l, wait_until, [&]() {
                return events.size() > 0;
            });
            tmp = std::move(events);
            events.clear();
        }
        Timestamp now(Timestamp::now());
        process_events(std::move(tmp));
        finalize_aggregators(now, needs_shutdown);
        wait_until = next_timestmamp(1s);
        if (needs_shutdown)
            break;
        prune_db();
    }
}
void Server::prune_db()
{
    using namespace std::chrono;
    using namespace std::chrono_literals;
    if (steady_clock::now() > nextPrune) {
        auto tx { db.create_transaction() };
        db.prune_minutes(Timestamp::now() - days(30));
        db.prune_hours(Timestamp::now() - days(30));
        tx.commit();
        nextPrune = steady_clock::now() + hours(1);
    }
}

void Server::finalize_aggregators(Timestamp t, bool drain)
{
    std::optional<SQLite::Transaction> tx;
    auto finalize {
        [&](auto& aggregator) {
            using agg_t = std::remove_cvref_t<decltype(aggregator)>::agg_t;
            aggregator.finalize_all(t, drain, [&](const std::string& host, std::vector<agg_t> buckets) {
                if (!tx)
                    tx.emplace(db.create_transaction());
                for (auto& b : buckets)
                    db.insert_aggregated(host, b);
            });
        }
    };
    finalize(aggregatorMinute);
    finalize(aggregatorHour);
    if (tx)
        tx->commit();
}

void Server::handle_event(Shutdown&&)
{
    needs_shutdown = true;
}

void Server::handle_event(Rx&& e)
{
    aggregatorMinute.process(e, true);
    aggregatorHour.process(e, true);
}

void Server::handle_event(Tx&& e)
{
    aggregatorMinute.process(e, false);
    aggregatorHour.process(e, false);
}

void Server::handle_event(GetAggregateHours&& e)
{
    auto agg { db.get_aggregated_hours(e.range) };
    aggregatorHour.merge_into(agg.byHost);
    e.cb(std::move(agg));
}

void Server::handle_event(GetAggregateMinutes&& e)
{
    auto agg { db.get_aggregated_minutes(e.range) };
    aggregatorMinute.merge_into(agg.byHost);
    e.cb(std::move(agg));
}
}
