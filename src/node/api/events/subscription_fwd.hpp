#pragma once
#include <memory>
namespace subscription {
namespace events {
    struct Event;
}
enum class Action{Subscribe, Unsubscribe};
}
struct SubscriptionData;
using subscription_ptr = std::shared_ptr<SubscriptionData>;
using subscription_data_ptr = SubscriptionData*;
struct SubscriptionRequest {
    subscription_ptr sptr;
    subscription::Action action;
};
