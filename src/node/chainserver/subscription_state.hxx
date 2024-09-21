#pragma once
#include "api/events/subscription.hpp"
#include "subscription_state.hpp"

inline void AddressSubscriptionState::session_end(auto accountStateGenerator, auto balanceFetcher)
{
    for (auto& [address, data] : sessionMap) {
        auto iter = map.find(address);
        assert(iter != map.end());
        auto subscriptions { select_id(iter->second.id) };
        if (data.forceReload) {
            accountStateGenerator(address).send(std::move(subscriptions));
        } else {
            assert(data.blocks.size() > 0);
            subscription::events::AccountDelta ad {
                .address { address },
                .newBalance { balanceFetcher(address) },
                .newTransactions { std::move(data.blocks) },
            };
            subscription::events::Event {
                std::move(ad)
            }
                .send(std::move(subscriptions));
        }
    }
    sessionMap.clear();
}
