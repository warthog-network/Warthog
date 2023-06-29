#pragma once
#include "subscription_declaration.hpp"
#include "mempool/order_key.hpp"

struct Conref;

namespace mempool{
struct Subscription {
private:
    SubscriptionMap m;
public:
    using iter_t = SubscriptionIter;
    auto& subscriptions() const{return m;}
    void set(std::optional<iter_t> iter, const mempool::OrderKey&, Conref);
    void erase(iter_t iter);
};
}
