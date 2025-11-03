#pragma once
#include "chainserver/db/types_fwd.hpp"
#include "db/sqlite_fwd.hpp"
#include "defi/order.hpp"


class OrderLoaderBase {
    friend chain_db::ChainDB;
    OrderLoaderBase(sqlite::Statement& stmt);

public:
    [[nodiscard]] wrt::optional<OrderData> operator()() const;
    OrderLoaderBase(const OrderLoaderBase&) = delete;
    OrderLoaderBase(OrderLoaderBase&& other);
    ~OrderLoaderBase();

private:
    sqlite::Statement* stmt;
    wrt::optional<OrderData> loaded;
};

template <bool ASCENDING>
class OrderLoader : public OrderLoaderBase {
    using OrderLoaderBase::OrderLoaderBase;
};

using OrderLoaderAscending = OrderLoader<true>;
using OrderLoaderDescending = OrderLoader<false>;
