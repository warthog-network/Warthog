#include "all.hpp"
#include "block/chain/history/history.hpp"
#include "chainserver/account_cache.hpp"
#include "general/address_funds.hpp"

namespace api {
void Block::set_reward(Reward r)
{
    if (actions.reward.has_value())
        throw std::runtime_error("Database error, each block can only have one reward transaction");
    actions.reward = r;
}

void Block::push_history(const Hash& txid,
    const std::vector<uint8_t>& data, chainserver::DBCache& c,
    PinFloor pinFloor)
{
    auto parsed = history::parse_throw(data);
    if (std::holds_alternative<history::WartTransferData>(parsed)) {
        auto& d = std::get<history::WartTransferData>(parsed);
        actions.wartTransfers.push_back(
            api::Block::Transfer {
                .fromAddress = c.accounts[d.fromAccountId],
                .fee = d.compactFee.uncompact(),
                .nonceId = d.pinNonce.id,
                .pinHeight = d.pinNonce.pin_height_from_floored(pinFloor),
                .txhash = txid,
                .toAddress = c.accounts[d.toAccountId],
                .amount = d.amount });
    } else if (std::holds_alternative<history::TokenTransferData>(parsed)) {
        auto& d = std::get<history::TokenTransferData>(parsed);
        auto& tokenData { c.tokens[d.tokenId] };
        actions.tokenTransfers.push_back(
            api::Block::TokenTransfer {
                .tokenInfo { tokenData },
                .fromAddress = c.accounts[d.fromAccountId],
                .fee = d.compactFee.uncompact(),
                .nonceId = d.pinNonce.id,
                .pinHeight = d.pinNonce.pin_height_from_floored(pinFloor),
                .txhash = txid,
                .toAddress = c.accounts[d.toAccountId],
                .amount = d.amount });
    } else {
        auto& d = std::get<history::RewardData>(parsed);
        auto toAddress = c.accounts[d.toAccountId];
        set_reward(Reward { txid, toAddress, d.miningReward });
    }
}
}
