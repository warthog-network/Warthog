#pragma once
#include "defi/order.hpp"

namespace sqlite{
struct Statement;
}

class ChainDB;
class OrderLoader {
    friend class ChainDB;
    OrderLoader(sqlite::Statement& stmt)
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
    sqlite::Statement* stmt;
    std::optional<OrderData> loaded;
};
