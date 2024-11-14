#pragma once
#include <memory>
#include <vector>
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

namespace subscription{
class SubscriptionVector {

private:
    using vector_t = std::vector<subscription_ptr>;
    vector_t data;

public:
    bool erase(subscription_data_ptr p);
    bool insert(subscription_ptr p);
    size_t size() const { return data.size(); }
    auto entries() const -> vector_t;
};
}
using SubscriptionVector = subscription::SubscriptionVector;
