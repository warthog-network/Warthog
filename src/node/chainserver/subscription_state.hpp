#pragma once
#include "api/events/subscription_fwd.hpp"
#include "api/types/forward_declarations.hpp"
#include "block/body/account_id.hpp"
#include "block/chain/header_chain.hpp"
#include "block/chain/height.hpp"
#include "crypto/address.hpp"
#include <list>
#include <map>
#include "wrt/optional.hpp"
#include <vector>

namespace subscription_state {

struct NewBlockInfo {
    wrt::optional<ShrinkInfo> rollback;
    std::vector<api::CompleteBlock>& newBlocks;
};
}
namespace chainserver {
class State;
}


namespace chain_subscription {
class ChainSubscriptionState {
public:
    void handle_subscription(SubscriptionRequest&&, const chainserver::State& s);
    size_t erase_all(subscription_data_ptr r);
    void on_chain_changed(const chainserver::State&, const subscription_state::NewBlockInfo& nbi);

private:
    SubscriptionVector subscriptions;
    static constexpr size_t num_latest_blocks { 10 };
    auto chain_state(const chainserver::State& s);
};
}

namespace minerdist_subscriptions {
struct Aggregator {
    void push_back(const Address& b);
    void push_front(const Address& b, bool trackDelta = true);
    void rollback(size_t by);
    void truncate(size_t maxlen = 1000);
    size_t size() const { return iters.size(); }

    void clear();
    [[nodiscard]] subscription::events::Event summarize();
    [[nodiscard]] subscription::events::Event state_event();

    struct Mapval {
        ssize_t count { 0 };
        ssize_t delta { 0 };
    };
    using map_t = std::map<Address, Mapval>;
    using iter_t = map_t::iterator;

private:
    void uncount_iter(iter_t iter);
    void count_new_iter(iter_t iter, bool trackDelta);
    map_t map;
    ssize_t nonzeroCount { 0 };
    ssize_t nonzeroDelta { 0 };
    std::list<map_t::iterator> iters;
};

class MinerdistSubscriptionState {
public:
    void handle_subscription(SubscriptionRequest&& r, const chainserver::State& s);
    void erase(subscription_data_ptr p);
    void on_chain_changed(const chainserver::State&, const subscription_state::NewBlockInfo&);

private:
    void insert(subscription_ptr p, const chainserver::State& s);

    void fill_aggregator(const chainserver::State& a);

    wrt::optional<Aggregator> aggregator;
    SubscriptionVector subscriptions;
    static constexpr size_t nMiners { 1000 };
};

}

namespace address_subscription {
struct SessionAddressCursor {
    api::Block& b;
};

struct SessionData {
    wrt::optional<SessionAddressCursor> session_cursor(const api::Block& b);
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
    void handle_subscription(SubscriptionRequest&&, const chainserver::State& s, const Address& a);
    size_t erase_all(subscription_data_ptr r);
    void on_chain_changed(const chainserver::State&, const subscription_state::NewBlockInfo& nbi);

private:
    bool insert(const subscription_ptr& r, const Address&);
    size_t erase(const subscription_ptr& r, const Address&);
    auto account_state(const Address&, const chainserver::State&);
    void session_start();
    void session_rollback(Height h);
    void session_block(const api::Block&);
    void session_end(const chainserver::State&);
    wrt::optional<SessionAddressCursor> session_address_cursor(const api::Block& b, const Address& a, Height);
};
}
using address_subscription::AddressSubscriptionState;
using chain_subscription::ChainSubscriptionState;
using minerdist_subscriptions::MinerdistSubscriptionState;
