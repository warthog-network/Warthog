#include "log_memory.hpp"
#include "api/events/subscription_fwd.hpp"
#include <iostream>

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
        std::lock_guard l(m);
        subscriptions.insert(r.sptr);
        using namespace subscription::events;
        Event { LogState({ logs }) }
            .send(std::move(r.sptr));
    }
    }
}

LogMemory::LogMemory()
    : datetimeFormatter { spdlog::details::make_unique<spdlog::pattern_formatter>("%b-%d %H:%M:%S.%e") }
{
}

void LogMemory::add_entry(const spdlog::details::log_msg& e)
{
    spdlog::memory_buf_t formatted;
    datetimeFormatter->format(e, formatted);
    std::string_view s(formatted.data(), formatted.size() - 1); //-1 to omit newline character

    using namespace std;
    LogEntry entry {
        .level = e.level,
        .tp { e.time },
        .payload { e.payload.data(), e.payload.size() },
        .datetime { std::string(s) }
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
