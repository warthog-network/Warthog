#pragma once
#include <map>

struct Conref;
namespace mempool {
struct OrderKey;
using SubscriptionMap = std::multimap<mempool::OrderKey, Conref>;
using SubscriptionIter = SubscriptionMap::iterator;
}
