#include "mining_subscription.hpp"
#include "communication/mining_task.hpp"
#include "chainserver/server.hpp"
#include <atomic>
namespace {
static std::atomic<uint64_t> gid { 0 };
}
namespace mining_subscription {

SubscriptionId::SubscriptionId()
    : id(gid++) {};

void MiningSubscriptions::dispatch(std::function<MiningTask(const Address&)> blockGenerator){
    for (auto &[addr,v] : subscriptions) {
        auto b{blockGenerator(addr)};
        for (auto &[_,f] : v) {
            f(MiningTask{b});
        }
    }
}

void MiningSubscriptions::subscribe(SubscriptionRequest&& r)
{
    auto [iter, _] = subscriptions.try_emplace(r.address);
    iter->second.push_back({ r.id, std::move(r.callback) });
    lookupSubscription.emplace(r.id, iter);
}

void MiningSubscriptions::unsubscribe(SubscriptionId id)
{
    auto iter { lookupSubscription.find(id) };
    if (iter == lookupSubscription.end())
        return;
    auto subiter { iter->second };
    assert(1 == std::erase_if(subiter->second, [id](Elem& e) { return e.id == id; }));
    if (subiter->second.size() == 0){
        subscriptions.erase(subiter);
    }
    lookupSubscription.erase(iter);
};

MiningSubscription::~MiningSubscription()
{
    auto l { chainServer.lock() };
    if (l)
        l->api_unsubscribe_mining(id);
}

}
