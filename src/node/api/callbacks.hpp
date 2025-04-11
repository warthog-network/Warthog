#pragma once
#include "api/types/forward_declarations.hpp"
#include "expected.hpp"
#include "general/result.hpp"
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
using ResultCb = std::function<void(const Result<void>&)>;
using ConnectedConnectionCB = std::function<void(const api::PeerinfoConnections&)>;
using BalanceCb = std::function<void(const Result<api::WartBalance>&)>;
using JSONCb = std::function<void(const Result<nlohmann::json>&)>;

using MempoolCb = std::function<void(const Result<api::MempoolEntries>&)>;
using MempoolInsertCb = std::function<void(const Result<TxHash>&)>;
using MempoolTxsCb = std::function<void(std::vector<std::optional<TransferTxExchangeMessage>>&)>;
using ChainMiningCb = std::function<void(const Result<ChainMiningTask>&)>;
using MiningCb = std::function<void(const Result<api::MiningState>&)>;
using TxcacheCb = std::function<void(const Result<chainserver::TransactionIds>&)>;
using HashrateCb = std::function<void(const Result<api::HashrateInfo>&)>;
using HashrateBlockChartCb = std::function<void(const Result<api::HashrateBlockChart>&)>;
using HashrateTimeChartCb = std::function<void(const Result<api::HashrateTimeChart>&)>;

using HeadCb = std::function<void(const Result<api::Head>&)>;
using ChainHeadCb = std::function<void(const Result<api::ChainHead>&)>;
using RoundCb = std::function<void(const Result<api::Round16Bit>&)>;
using HeaderdownloadCb = std::function<void(const HeaderDownload::Downloader&)>;
using HeaderCb = std::function<void(const Result<std::pair<NonzeroHeight, Header>>&)>;
using HashCb = std::function<void(const Result<Hash>&)>;
using GridCb = std::function<void(const Result<Grid>&)>;
using TxCb = std::function<void(const Result<api::Transaction>&)>;
using LatestTxsCb = std::function<void(const Result<api::TransactionsByBlocks>&)>;
using BlockCb = std::function<void(const Result<api::Block>&)>;
using HistoryCb = std::function<void(const Result<api::AccountHistory>&)>;
using RichlistCb = std::function<void(const Result<api::Richlist>&)>;

using VersionCb = std::function<void(const Result<PrintNodeVersion>&)>;
using WalletCb = std::function<void(const Result<api::Wallet>&)>;
using RawCb = std::function<void(const api::Raw&)>;
using DBSizeCB = std::function<void(const Result<api::DBSize>&)>;
using InfoCb = std::function<void(const Result<api::NodeInfo>&)>;
using SampledPeersCb = std::function<void(const std::vector<TCPPeeraddr>&)>;
using TransmissionCb = std::function<void(const api::TransmissionTimeseries&)>;
