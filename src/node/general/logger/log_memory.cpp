#include "log_memory.hpp"
#include "api/events/subscription_fwd.hpp"

namespace logging {
void LogMemory::subscribe(SubscriptionRequest&& r)
{
    using enum subscription::Action;
    switch (r.action) {
    case Unsubscribe: {
        std::lock_guard l(m);
        subscriptions.erase(r.sptr.get());
        break;
    }
    case Subscribe: {
        auto inserted = [&]() {
            std::lock_guard l(m);
            return subscriptions.insert(r.sptr);
        }();
        using namespace subscription::events;
        if (inserted) {
            Event { LogState({ logs }) }
                .send(std::move(r.sptr));
        }
    }
    }
}
void LogMemory::add_entry(const spdlog::details::log_msg& e)
{
    LogEntry entry {
        .level = e.level,
        .tp { e.time },
        .payload { e.payload.data(), e.payload.size() }
    };
    {
        std::lock_guard l(m);
        logs.push_back(entry);
        if (logs.size() > 1200) {
            logs.erase(logs.begin(), logs.begin() + 200);
        }
        using namespace subscription::events;

        if (subscriptions.size() > 0) {
            Event {
                LogLine { entry }
            }.send(subscriptions.entries());
        }
    }
}
}
