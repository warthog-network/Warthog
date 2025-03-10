#include "order_loader.hpp"
#include "db/sqlite_fwd.hpp"

std::optional<OrderData> OrderLoader::operator()() const
{
    std::optional<OrderData> res;
    auto r { stmt->next_row() };
    if (r.has_value()) {
        res = OrderData { r[0], r[1], r[2], r[3], r[4] };
    }
    return res;
}


OrderLoader::~OrderLoader()
{
    if (stmt)
        stmt->reset();
}
