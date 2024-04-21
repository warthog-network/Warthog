#pragma once
#include <map>

class Conref;
namespace mempool {
struct OrderKey;
using SubscriptionMap = std::multimap<mempool::OrderKey, Conref>;
using SubscriptionIter = SubscriptionMap::iterator;
}
