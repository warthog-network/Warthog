#include "subscription.hpp"
#include "eventloop/types/conref_declaration.hpp"

namespace mempool {
void Subscription::set(std::optional<Subscription::iter_t> iter, const mempool::OrderKey& k, Conref cr)
{
    if (iter)
        m.erase(*iter);
    m.insert({ k, cr });
};
void Subscription::erase(iter_t iter)
{
    m.erase(iter);
};
}
