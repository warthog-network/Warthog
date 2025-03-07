#pragma once
#include "defi/order.hpp"

struct Statement2;
class ChainDB;
class OrderLoader {
    friend class ChainDB;
    OrderLoader(Statement2& stmt)
        : stmt(&stmt)
    {
    }

public:
    [[nodiscard]] std::optional<OrderData> operator()() const;

    OrderLoader(const OrderLoader&) = delete;
    OrderLoader(OrderLoader&& other)
    {
        stmt = other.stmt;
        other.stmt = nullptr;
    }
    ~OrderLoader();

private:
private:
    Statement2* stmt;
    std::optional<OrderData> loaded;
};
