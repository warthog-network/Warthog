#pragma once
#include "api/events/subscription_fwd.hpp"
#include "api/types/forward_declarations.hpp"
#include "block/chain/height.hpp"
#include "crypto/address.hpp"
#include <map>
#include <optional>
#include <vector>

namespace chain_subscription {
class ChainSubscriptionState {
private:
    using vector_t = std::vector<subscription_ptr>;
    vector_t subscriptions;

public:
    size_t size() const { return subscriptions.size(); }
    auto get_subscriptions() const ->vector_t;
    void erase(subscription_data_ptr p);
    bool insert(subscription_ptr p);

};

}

namespace address_subscription {
struct SessionAddressCursor {
    api::Block& b;
};

struct SessionData {
    std::optional<SessionAddressCursor> session_cursor(const api::Block& b);
    bool forceReload { false }; // whether the history must be reloaded
                                // (in case of > 100 entries
                                // or dirty from rollback)
    uint16_t i = 0;
    std::vector<api::Block> blocks;
};

class AddressSubscriptionState {

private:
    using id_t = size_t;
    std::vector<subscription_ptr> select_id(id_t id);

    id_t valIndexCounter { 0 };
    id_t sessionId { 1 };
    using session_map_t = std::map<Address, SessionData>;
    session_map_t sessionMap;
    struct MapVal {
        MapVal(id_t& idCounter)
            : id(++idCounter) {};
        id_t id;
        size_t counter { 0 };
        Height latestTransactionHeight { 0 };
        id_t sessionId;
        session_map_t::iterator sessionMapIter;
    };
    std::map<Address, MapVal> map;
    using elem_t = std::tuple<id_t, subscription_ptr, Address>;
    std::vector<elem_t> subscriptions;

private:
    auto& get_session_data(MapVal& mapVal, const Address& a);

public:
    bool insert(const subscription_ptr& r, const Address&);
    size_t erase_all(subscription_data_ptr r);
    size_t erase(const subscription_ptr& r, const Address&);

    void session_start();
    void session_rollback(Height h);
    void session_block(const api::Block&);
    void session_end(auto accountStateGenerator, auto balanceFetcher);

private:
    std::optional<SessionAddressCursor> session_address_cursor(const api::Block& b, const Address& a, Height);
};
}
using address_subscription::AddressSubscriptionState;
using chain_subscription::ChainSubscriptionState;
