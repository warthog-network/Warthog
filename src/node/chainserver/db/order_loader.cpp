#include "order_loader.hpp"
#include "chainserver/db/statement.hpp"

std::optional<OrderData> OrderLoader::load_next()
{
    auto r { Statement2::Row(*stmt) };
    if (!r.has_value())
        return {};
    return OrderData { r[0], r[1], r[2], r[3], r[4] };
}

OrderLoader::~OrderLoader()
{
    if(stmt)
        stmt->reset();
}
