#pragma once

#include "api/events/subscription.hpp"
#include "api/events/subscription_fwd.hpp"
#include "log_entry.hpp"
#include "spdlog/spdlog.h"

namespace logging {
struct LogMemory {

    using callback_t = std::function<void(const LogEntry&)>;
    void add_entry(const spdlog::details::log_msg& e);
    void subscribe(SubscriptionRequest&& r);

    LogMemory();
private:
    std::unique_ptr<spdlog::pattern_formatter> datetimeFormatter;
    std::recursive_mutex m;
    std::vector<LogEntry> logs;
    SubscriptionVector subscriptions;
};
inline LogMemory logMemory;
}
