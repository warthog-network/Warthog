#pragma once
#include "api/types/forward_declarations.hpp"
#include "expected.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// forward declarations
class TransferTxExchangeMessage;
class Hash;
class PrivKey;
class TxHash;
class Grid;
class NonzeroHeight;
struct ChainMiningTask;
struct Error;
namespace HeaderDownload {
class Downloader;
}
namespace chainserver {
struct TransactionIds;
}
struct NodeVersion {
    /* data */
};


class Header;

using PeersCb = std::function<void(const std::vector<API::Peerinfo>&)>;
using SyncedCb = std::function<void(bool)>;
using ResultCb = std::function<void(const tl::expected<void, Error>&)>;
using ConnectedConnectionCB = std::function<void(const API::PeerinfoConnections&)>;
using BalanceCb = std::function<void(const tl::expected<API::Balance, Error>&)>;

// using OffensesCb = std::function<void(const tl::expected<std, Error>&)>;
using MempoolCb = std::function<void(const tl::expected<API::MempoolEntries, Error>&)>;
using MempoolInsertCb = std::function<void(const tl::expected<TxHash, Error>&)>;
using MempoolTxsCb = std::function<void(std::vector<std::optional<TransferTxExchangeMessage>>&)>;
using ChainMiningCb = std::function<void(const tl::expected<ChainMiningTask, Error>&)>;
using MiningCb = std::function<void(const tl::expected<API::MiningState, Error>&)>;
using TxcacheCb = std::function<void(const tl::expected<chainserver::TransactionIds, Error>&)>;
using HashrateCb = std::function<void(const tl::expected<API::HashrateInfo, Error>&)>;
using HashrateChartCb = std::function<void(const tl::expected<API::HashrateChart, Error>&)>;

using HeadCb = std::function<void(const tl::expected<API::Head, Error>&)>;
using ChainHeadCb = std::function<void(const tl::expected<API::ChainHead, Error>&)>;
using RoundCb = std::function<void(const tl::expected<API::Round16Bit, Error>&)>;
using HeaderdownloadCb = std::function<void(const HeaderDownload::Downloader&)>;
using HeaderCb = std::function<void(const tl::expected<std::pair<NonzeroHeight,Header>, Error>&)>;
using HashCb = std::function<void(const tl::expected<Hash, Error>&)>;
using GridCb = std::function<void(const tl::expected<Grid, Error>&)>;
using TxCb = std::function<void(const tl::expected<API::Transaction, Error>&)>;
using LatestTxsCb = std::function<void(const tl::expected<API::TransactionsByBlocks, Error>&)>;
using BlockCb = std::function<void(const tl::expected<API::Block, Error>&)>;
using HistoryCb = std::function<void(const tl::expected<API::AccountHistory, Error>&)>;
using RichlistCb = std::function<void(const tl::expected<API::Richlist, Error>&)>;

using VersionCb = std::function<void(const tl::expected<NodeVersion, Error>&)>;
using WalletCb = std::function<void(const tl::expected<API::Wallet, Error>&)>;
using RawCb = std::function<void(const API::Raw&)>;
