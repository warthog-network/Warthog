#pragma once
#include "chainserver/db/types_fwd.hpp"
#include "db/sqlite_fwd.hpp"
#include "defi/order.hpp"

namespace sqlite {
class Statement;
}

template <bool ASCENDING>
class OrderLoader {
    friend chain_db::ChainDB;
    OrderLoader(sqlite::Statement& stmt)
        : stmt(&stmt)
    {
    }

public:
    [[nodiscard]] std::optional<OrderData> operator()() const
    {
        std::optional<OrderData> res;
        auto r { stmt->next_row() };
        if (r.has_value()) {
            TransactionId txid {
                TransactionId::Generator {
                    .accountId = r[1],
                    .pinHeight = r[2],
                    .nonceId = r[3] }
            };
            res = OrderData { r[0], txid, r[4], r[5], r[6] };
        }
        return res;
    }

    OrderLoader(const OrderLoader&) = delete;
    OrderLoader(OrderLoader&& other)
    {
        stmt = other.stmt;
        other.stmt = nullptr;
    }
    ~OrderLoader()
    {
        if (stmt)
            stmt->reset();
    }

private:
    sqlite::Statement* stmt;
    std::optional<OrderData> loaded;
};

using OrderLoaderAscending = OrderLoader<true>;
using OrderLoaderDescending = OrderLoader<false>;
