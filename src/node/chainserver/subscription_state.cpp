#include "subscription_state.hpp"
#include "api/types/all.hpp"
#include "spdlog/spdlog.h"
#include <optional>

namespace chain_subscription {
void ChainSubscriptionState::erase(subscription_data_ptr p)
{
    std::erase_if(subscriptions, [&](subscription_ptr& p2) {
        return p == p2.get();
    });
}
auto ChainSubscriptionState::get_subscriptions() const -> vector_t
{
    return subscriptions;
}
bool ChainSubscriptionState::insert(subscription_ptr p)
{
    for (auto& p2 : subscriptions) {
        if (p2.get() == p.get()) {
            return false;
        }
    }
    subscriptions.push_back(std::move(p));
    return true;
}
}
namespace address_subscription {
std::optional<SessionAddressCursor> SessionData::session_cursor(const api::Block& b)
{
    if (forceReload)
        return {};
    if (++i > 100) {
        blocks.clear();
        forceReload = true;
        return {};
    }
    if (blocks.size() == 0 || blocks.back().height < b.height) {

        blocks.push_back({ b.header, b.height, b.confirmations });
    }
    return SessionAddressCursor { blocks.back() };
}
auto& AddressSubscriptionState::get_session_data(MapVal& mapVal, const Address& a)
{
    if (mapVal.sessionId != sessionId) {
        mapVal.sessionId = sessionId;
        mapVal.sessionMapIter = sessionMap.try_emplace(a).first;
    };
    return mapVal.sessionMapIter->second;
}
bool AddressSubscriptionState::insert(const subscription_ptr& sptr, const Address& a)
{
    auto p { map.try_emplace(a, valIndexCounter) };
    auto newAddress { p.second };
    auto& mapVal { p.first->second };
    auto id { mapVal.id };
    if (!newAddress) {
        for (auto& [id2, sptr2, addr] : subscriptions) {
            if (id2 == id && sptr2.get() == sptr.get())
                return false;
        }
    }
    mapVal.counter += 1;
    subscriptions.push_back({ id, sptr, a });
    return true;
}

size_t AddressSubscriptionState::erase_all(subscription_data_ptr r)
{
    auto n { std::erase_if(subscriptions, [&](elem_t& e) {
        auto& [id, sptr, addr] = e;
        if (sptr.get() == r) {
            auto iter = map.find(addr);
            assert(iter != map.end());
            assert(iter->second.counter > 0);
            iter->second.counter -= 1;
            if (iter->second.counter == 0)
                map.erase(iter);
            return true;
        }
        return false;
    }) };

    return n;
}

size_t AddressSubscriptionState::erase(const subscription_ptr& sptr, const Address& addr)
{
    auto iter { map.find(addr) };
    if (iter == map.end())
        return 0;
    const auto id { iter->second.id };
    return std::erase_if(subscriptions, [&](elem_t& e) {
        auto& [id2, sptr2, addr2] = e;
        if (id == id2 && sptr.get() == sptr2.get()) {
            assert(iter->second.counter > 0);
            iter->second.counter -= 1;
            if (iter->second.counter == 0)
                map.erase(iter);
            return true;
        }
        return false;
    });
}

// session functions
void AddressSubscriptionState::session_start()
{
    sessionId += 1;
    sessionMap.clear();
}
void AddressSubscriptionState::session_rollback(Height h)
{
    for (auto& [addr, mapval] : map) {
        if (h < mapval.latestTransactionHeight) {
            get_session_data(mapval, addr).forceReload = true;
        }
    }
}
void AddressSubscriptionState::session_block(const api::Block& b)
{
    if (auto& r { b.reward() }) {
        if (auto c { session_address_cursor(b, r->toAddress, b.height) })
            c->b.set_reward(*r);
    }
    for (auto& t : b.transfers) {
        if (auto c { session_address_cursor(b, t.toAddress, b.height) })
            c->b.transfers.push_back(t);
        if (auto c { session_address_cursor(b, t.fromAddress, b.height) })
            c->b.transfers.push_back(t);
    }
}
std::optional<SessionAddressCursor> AddressSubscriptionState::session_address_cursor(const api::Block& b, const Address& a, Height h)
{
    auto iter { map.find(a) };
    if (iter == map.end())
        return {};
    auto& mapVal { iter->second };
    mapVal.latestTransactionHeight = h;

    return get_session_data(mapVal, a).session_cursor(b);
}

std::vector<subscription_ptr> AddressSubscriptionState::select_id(id_t id)
{
    std::vector<subscription_ptr> res;
    for (auto& [id2, ptr, addr] : subscriptions) {
        if (id == id2) {
            res.push_back(ptr);
        }
    }
    return res;
}
}
