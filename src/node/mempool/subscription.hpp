#pragma once
#include "subscription_declaration.hpp"
#include "mempool/order_key.hpp"

class Conref;

namespace mempool{
struct Subscription {
private:
    SubscriptionMap m;
public:
    using iter_t = SubscriptionIter;
    auto& subscriptions() const{return m;}
    void set(wrt::optional<iter_t> iter, const mempool::OrderKey&, Conref);
    void erase(iter_t iter);
};
}
