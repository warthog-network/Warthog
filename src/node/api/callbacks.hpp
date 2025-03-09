#pragma once
#include "api/types/forward_declarations.hpp"
#include "expected.hpp"
#include <nlohmann/json_fwd.hpp>
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
struct PrintNodeVersion {
};


class Header;
struct TCPPeeraddr;

using PeersCb = std::function<void(const std::vector<api::Peerinfo>&)>;
using IpCounterCb = std::function<void(const api::IPCounter&)>;
using ThrottledCb = std::function<void(const std::vector<api::ThrottledPeer>&)>;
using SyncedCb = std::function<void(bool)>;
using ResultCb = std::function<void(const tl::expected<void, Error>&)>;
using ConnectedConnectionCB = std::function<void(const api::PeerinfoConnections&)>;
using BalanceCb = std::function<void(const tl::expected<api::Balance, Error>&)>;
using JSONCb = std::function<void(const tl::expected<nlohmann::json, Error>&)>;

// using OffensesCb = std::function<void(const tl::expected<std, Error>&)>;
using MempoolCb = std::function<void(const tl::expected<api::MempoolEntries, Error>&)>;
using MempoolInsertCb = std::function<void(const tl::expected<TxHash, Error>&)>;
using MempoolTxsCb = std::function<void(std::vector<std::optional<TransferTxExchangeMessage>>&)>;
using ChainMiningCb = std::function<void(const tl::expected<ChainMiningTask, Error>&)>;
using MiningCb = std::function<void(const tl::expected<api::MiningState, Error>&)>;
using TxcacheCb = std::function<void(const tl::expected<chainserver::TransactionIds, Error>&)>;
using HashrateCb = std::function<void(const tl::expected<api::HashrateInfo, Error>&)>;
using HashrateBlockChartCb = std::function<void(const tl::expected<api::HashrateBlockChart, Error>&)>;
using HashrateTimeChartCb = std::function<void(const tl::expected<api::HashrateTimeChart, Error>&)>;

using HeadCb = std::function<void(const tl::expected<api::Head, Error>&)>;
using ChainHeadCb = std::function<void(const tl::expected<api::ChainHead, Error>&)>;
using RoundCb = std::function<void(const tl::expected<api::Round16Bit, Error>&)>;
using HeaderdownloadCb = std::function<void(const HeaderDownload::Downloader&)>;
using HeaderCb = std::function<void(const tl::expected<std::pair<NonzeroHeight, Header>, Error>&)>;
using HashCb = std::function<void(const tl::expected<Hash, Error>&)>;
using GridCb = std::function<void(const tl::expected<Grid, Error>&)>;
using TxCb = std::function<void(const tl::expected<api::Transaction, Error>&)>;
using LatestTxsCb = std::function<void(const tl::expected<api::TransactionsByBlocks, Error>&)>;
using BlockCb = std::function<void(const tl::expected<api::Block, Error>&)>;
using HistoryCb = std::function<void(const tl::expected<api::AccountHistory, Error>&)>;
using RichlistCb = std::function<void(const tl::expected<api::Richlist, Error>&)>;

using VersionCb = std::function<void(const tl::expected<PrintNodeVersion, Error>&)>;
using WalletCb = std::function<void(const tl::expected<api::Wallet, Error>&)>;
using RawCb = std::function<void(const api::Raw&)>;
using DBSizeCB = std::function<void(const tl::expected<api::DBSize, Error>&)>;
using InfoCb = std::function<void(const tl::expected<api::NodeInfo, Error>&)>;
using SampledPeersCb = std::function<void(const std::vector<TCPPeeraddr>&)>;
using TransmissionCb = std::function<void(const api::TransmissionTimeseries&)>;
