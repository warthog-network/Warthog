#include "order_loader.hpp"
#include "db/sqlite.hpp"
#include "db/type_conv.hpp"

OrderLoaderBase::OrderLoaderBase(sqlite::Statement& stmt)
    : stmt(&stmt)
{
}
std::optional<OrderData> OrderLoaderBase::operator()() const
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
OrderLoaderBase::OrderLoaderBase(OrderLoaderBase&& other)
{
    stmt = other.stmt;
    other.stmt = nullptr;
}
OrderLoaderBase::~OrderLoaderBase()
{
    if (stmt)
        stmt->reset();
}
