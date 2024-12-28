#pragma once
#include "price.hpp"

#include <cstdint>
namespace defi{
    struct Order {
        uint64_t amount;
        Price limit;
        Order(uint64_t amount, Price limit) : amount(amount), limit(limit) {}
    };
    struct PullBaseOrder : public Order { // sells quote
        using Order::Order;
    };
    struct PullQuoteOrder : public Order { // sells base
        using Order::Order;
    };
    struct PushBaseOrder : public Order { // sells base
        using Order::Order;
    };
    struct PushQuoteOrder : public Order { // sells quote
        using Order::Order;
    };
}
