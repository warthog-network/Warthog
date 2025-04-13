#pragma once

#include "api/callbacks.hpp"
#include "block/block.hpp"
#include "crypto/address.hpp"
#include <map>
#include <vector>
class ChainServer;

namespace mining_subscription {
using callback_t = std::function<void(Result<ChainMiningTask>&&)>;
struct SubscriptionId {
    auto operator<=>(const SubscriptionId&) const = default;
    SubscriptionId();

    auto val() const { return id; }

private:
    uint64_t id;
};

struct SubscriptionRequest {
    SubscriptionId id;
    Address address;
    callback_t callback;
    static SubscriptionRequest make(Address address, callback_t callback)
    {
        return { {}, address, callback };
    }
};

class MiningSubscriptions {
public:
    void subscribe(SubscriptionRequest&&);
    void unsubscribe(SubscriptionId);
    void dispatch(std::function<Result<ChainMiningTask>(const Address&)> blockGenerator);

private:
    struct Elem {
        SubscriptionId id;
        callback_t callback;
    };
    std::map<Address, std::vector<Elem>> subscriptions;
    std::map<SubscriptionId, decltype(subscriptions)::iterator> lookupSubscription;
};

struct MiningSubscription {
    friend class ::ChainServer;

private:
    std::weak_ptr<ChainServer> chainServer;
    mining_subscription::SubscriptionId id;
    MiningSubscription(std::shared_ptr<ChainServer> chainServer, mining_subscription::SubscriptionId id)
        : chainServer(std::move(chainServer))
        , id(id)
    {
    }

public:
    MiningSubscription(const MiningSubscription&) = delete;
    MiningSubscription(MiningSubscription&&) = default;
    ~MiningSubscription();
};

}

using MiningSubscriptions = mining_subscription::MiningSubscriptions;
