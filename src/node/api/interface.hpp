#pragma once

#include "transport/tcp/conman.hpp"
#include "callbacks.hpp"
#include "chainserver/mining_subscription.hpp"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"
#include "events/subscription_fwd.hpp"

// mempool cbunctions
void put_mempool(PaymentCreateMessage&&, MempoolInsertCb);
void get_mempool(MempoolCb cb);
void lookup_tx(const Hash hash, TxCb f);

void get_latest_transactions(LatestTxsCb f);

// peer db functions
void get_banned_peers(PeerServer::banned_callback_t&& cb);
void unban_peers(ResultCb&& cb);
void get_connection_schedule(JSONCb&& cb);

inline void get_offenses(Page page, PeerServer::offenses_callback_t&& cb)
{
    global().peerServer->async_get_offenses(page, std::move(cb));
}

// void get_connected_peers(Conman::PeersCB cb);

void get_failed_addresses(PeerServer::banned_callback_t cb);
void get_verified_addresses(PeerServer::banned_callback_t cb);

void get_connected_peers2(PeersCb&& cb);
void disconnect_peer(uint64_t, ResultCb&& cb);
void get_connected_connection(ConnectedConnectionCB&& cb);

// tools functions
void get_round16bit_e8(uint64_t e8, RoundCb cb);
void get_round16bit_funds(Funds e8, RoundCb cb);
void get_version(VersionCb cb);
void get_wallet_new(WalletCb cb);
void get_wallet_from_privkey(const PrivKey& pk, WalletCb cb);
void get_janushash_number(std::string_view, RawCb cb);

// chain functions
void get_block_head(HeadCb cb);
void get_chain_mine(const Address& a, MiningCb cb);
mining_subscription::MiningSubscription subscribe_chain_mine(Address address, mining_subscription::callback_t callback);
void get_chain_header(api::HeightOrHash, HeaderCb cb);
void get_chain_hash(Height height, HashCb cb);
void get_chain_grid(GridCb cb);
void get_chain_block(api::HeightOrHash, BlockCb cb);
void get_txcache(TxcacheCb&& cb);
void get_hashrate_n(size_t n, HashrateCb&& cb);
void get_hashrate(HashrateCb&& cb);
void get_hashrate_block_chart(NonzeroHeight from, NonzeroHeight to, size_t window, HashrateBlockChartCb&& cb);
void get_hashrate_time_chart(uint32_t from, uint32_t to, size_t window, HashrateTimeChartCb&& cb);
void put_chain_append(ChainMiningTask&& mt, ResultCb cb);
void get_signed_snapshot(Eventloop::SignedSnapshotCb&& cb);

// sync functions
void get_headerdownload(HeaderdownloadCb f);

// account functions
void get_account_balance(const api::AccountIdOrAddress& address, BalanceCb cb);
void get_account_history(const Address& address, uint64_t end, HistoryCb cb);
void get_account_richlist(RichlistCb cb);

// endpoints function
void inspect_eventloop(std::function<void(const Eventloop& e)>&&);

void subscribe_chain_event(SubscriptionRequest);
void subscribe_connection_event(SubscriptionRequest);
void subscribe_account_event(SubscriptionRequest, Address);
void subscribe_minerdist_event(SubscriptionRequest);
void destroy_all_subscriptions(subscription_data_ptr);
