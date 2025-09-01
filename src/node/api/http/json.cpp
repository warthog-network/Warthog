#include "json.hpp"
#include "api/types/all.hpp"
#include "block/header/header_impl.hpp"
#include "block/header/view.hpp"
#include "chainserver/transaction_ids.hpp"
#include "communication/mining_task.hpp"
#include "communication/rxtx_server/api_types.hpp"
#include "crypto/crypto.hpp"
#include "eventloop/eventloop.hpp"
#include "eventloop/sync/header_download/header_download.hpp"
#include "eventloop/sync/sync.hpp"
#include "general/errors.hpp"
#include "general/hex.hpp"
#include "general/is_testnet.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include <nlohmann/json.hpp>
#include <ranges>

using namespace std::chrono;
using namespace nlohmann;
namespace {
std::string format_utc(uint32_t timestamp)
{
    std::chrono::system_clock::time_point tp { std::chrono::seconds(timestamp) };
    auto tt { std::chrono::system_clock::to_time_t(tp) };
    auto utc_f = *std::gmtime(&tt);
    std::string out;
    out.resize(30);
    auto len { std::strftime(&out[0], out.size(), "%F %T UTC", &utc_f) };
    out.resize(len);
    return out;
}
json to_json_temporal(const api::TemporalInfo& tx)
{
    return {
        { "confirmations", tx.confirmations },
        { "blockHeight", tx.height },
        { "utc", format_utc(tx.timestamp) },
        { "timestamp", tx.timestamp },
    };
}
}
struct Inspector {
    static auto header_download(const Eventloop& e)
    {
        auto& d { e.headerDownload };
        json j;
        j["minWork"] = d.minWork.to_string();
        j["verifierMapSize"] = d.verifierMap.size();
        j["leaderListSize"] = d.leaderList.size();
        j["config"] = json {
            { "maxLeaders", d.maxLeaders },
            { "pendingDepth", d.pendingDepth }
        };
        {
            json qb;
            for (auto& [header, node] : d.queuedBatches) {
                qb[serialize_hex(header)]
                    = json {
                          { "batchSize", node.batch.size() },
                          { "originId", node.originId },
                          { "probeRefsSize", node.probeRefs.size() },
                          { "leaderRefsSize", node.leaderRefs.size() }
                      };
            }
            j["queuedBatches"] = qb;
        }
        return j.dump(1);
    }
    // static std::string endpoint_timers(const Eventloop& c)
    // {
    //     auto now = steady_clock::now();
    //     auto& m = c.connections;
    //     using VerIter = decltype(c.connections)::VerIter;
    //     using PinIter = decltype(c.connections)::PinIter;
    //     json j;
    //     json verifiers = json::array();
    //     json pins = json::array();
    //     for (auto& [t, v] : m.timer) {
    //         json e;
    //         e["expiresSeconds"] = duration_cast<seconds>(t - now).count();
    //         if (std::holds_alternative<VerIter>(v)) {
    //             auto& iter = std::get<VerIter>(v);
    //             auto& n = iter->second;
    //             e["endpoint"] = iter->first.to_string();
    //             e["seenSecondsAgo"] = n.outboundConnection ? 0 : duration_cast<seconds>(now - n.lastVerified).count();
    //             verifiers.push_back(e);
    //         } else {
    //             assert(std::holds_alternative<PinIter>(v));
    //             auto& iter = std::get<PinIter>(v);
    //             e["endpoint"] = iter->first.to_string();
    //             e["sleepOnFailedSeconds"] = iter->second.sleepSeconds;
    //             pins.push_back(e);
    //         }
    //     }
    //     j["verifiers"] = verifiers;
    //     j["pins"] = pins;
    //     return j.dump(1);
    // }
};

namespace jsonmsg {
namespace {
template <typename T>
json verified_json(const std::map<TCPPeeraddr, T>& map)
{
    using namespace std::chrono;
    auto now = steady_clock::now();
    json e = json::array();
    for (auto& [a, n] : map) {
        json j;
        j["endpoint"] = a.to_string();
        j["seenSecondsAgo"] = n.outboundConnection ? 0 : duration_cast<seconds>(now - n.lastVerified).count();
        j["outboundConnection"] = n.outboundConnection;

        e.push_back(j);
    }
    return e;
}
json endpoint_json(auto& v)
{
    json e = json::array();
    for (const auto& ae : v) {
        e.push_back(ae.to_string());
    }
    return e;
}

json grid_json(const Grid& g)
{
    json out = json::array();
    for (auto header : g) {
        out.push_back(serialize_hex(header));
    }
    return out;
}

json header_json(const Header& header, NonzeroHeight height)
{
    auto version { header.version() };
    const bool testnet { is_testnet() };
    auto powVersion { POWVersion::from_params(height, version, testnet) };
    assert(powVersion.has_value());
    bool verusV2_2 { powVersion->uses_verus_2_2() };
    auto verusHash { verusV2_2 ? verus_hash_v2_2(header) : verus_hash_v2_1(header) };
    auto blockHash { header.hash() };
    auto sha256tHash { hashSHA256(blockHash) };
    auto target { header.target(height, testnet) };
    uint32_t targetBE = hton32(target.binary());
    json h;
    h["raw"] = serialize_hex(header.data(), header.size());
    h["timestamp"] = header.timestamp();
    h["utc"] = format_utc(header.timestamp());
    h["target"] = serialize_hex(targetBE);
    h["difficulty"] = target.difficulty();
    h["hash"] = serialize_hex(header.hash());
    h["pow"] = json {
        { "verusV2.2", verusV2_2 },
        { "hashVerus", serialize_hex(verusHash) },
        { "hashSha256t", serialize_hex(sha256tHash) },
        { "floatVerus", CustomFloat(verusHash).to_double() },
        { "floatSha256t", CustomFloat(sha256tHash).to_double() },
    };
    h["merkleroot"] = serialize_hex(header.merkleroot());
    h["nonce"] = serialize_hex(header.nonce());
    h["prevHash"] = serialize_hex(header.prevhash());
    h["version"] = serialize_hex(version.value());
    return h;
}

[[nodiscard]] json body_json(const api::Block& b)
{
    json out;
    auto& actions { b.actions };
    { // rewards
        json a = json::array();
        if (b.actions.reward) {
            auto& r { *b.actions.reward };
            json elem;
            elem["txHash"] = serialize_hex(r.txhash);
            elem["toAddress"] = r.toAddress.to_string();
            elem["amount"] = to_json(r.wart);
            a.push_back(elem);
        }
        out["rewards"] = a;
    }

    { // WART transfers
        json a = json::array();
        for (auto& t : actions.wartTransfers) {
            json elem;
            elem["fromAddress"] = t.originAddress.to_string();
            elem["fee"] = to_json(t.fee);
            elem["nonceId"] = t.nonceId;
            elem["pinHeight"] = t.pinHeight;
            elem["txHash"] = serialize_hex(t.txhash);
            elem["toAddress"] = t.toAddress.to_string();
            elem["amount"] = to_json(t.amount);
            a.push_back(elem);
        }
        out["wartTransfers"] = a;
    }
    { // Token transfers
        json a = json::array();
        for (auto& t : actions.tokenTransfers) {
            json elem;
            elem["token"] = to_json(t.assetInfo);
            elem["fromAddress"] = t.originAddress.to_string();
            elem["fee"] = to_json(t.fee);
            elem["nonceId"] = t.nonceId;
            elem["pinHeight"] = t.pinHeight;
            elem["txHash"] = serialize_hex(t.txhash);
            elem["toAddress"] = t.toAddress.to_string();
            elem["amount"] = to_json(t.amount);
            a.push_back(elem);
        }
        out["tokenTransfers"] = a;
    }
    { // Token transfers
        json a = json::array();
        for (auto& o : actions.newOrders) {
            json elem;
            elem["token"] = to_json(o.assetInfo);
            elem["fee"] = o.fee;
            elem["amount"] = to_json(o.amount);
            elem["limit"] = o.limit.to_double();
            elem["buy"] = o.buy;
            elem["txhash"] = serialize_hex(o.txhash);
            elem["address"] = o.originAddress.to_string();
            a.push_back(elem);
        }
        out["newOrders"] = a;
    }
    { // Matches
        json a = json::array();
        for (auto& s : actions.matches) {
            // TxHash txhash;
            // std::vector<Swap> buySwaps;
            // std::vector<Swap> sellSwaps;
            auto bq_json {
                [&](const auto& bq) -> json {
                    return {
                        { "base", to_json(bq.base().to_decimal(s.assetInfo.precision)) },
                        { "quote", to_json(bq.quote()) }
                    };
                }
            };
            json elem;
            elem["asset"] = jsonmsg::to_json(s.assetInfo);
            elem["txhash"] = serialize_hex(s.txhash);
            elem["poolBefore"] = bq_json(s.poolBefore);
            elem["poolAfter"] = bq_json(s.poolAfter);
            auto match_json {
                [&]<typename T>(const std::vector<T>& v) {
                    json res = json::array();
                    for (auto& s : v) {
                        auto e { bq_json(s) };
                        e["historyId"] = s.referred_history_id().value();
                        res.push_back(std::move(e));
                    }
                    return res;
                }
            };
            elem["buySwapsj"] = match_json(s.buySwaps);
            elem["sellSwaps"] = match_json(s.sellSwaps);
            elem["txhash"] = serialize_hex(s.txhash);
            a.push_back(elem);
        }
        out["swaps"] = a;
    }
    return out;
}

} // namespace

using namespace nlohmann;

json to_json(Wart w)
{
    return {
        { "str", w.to_string() },
        { "E8", w.E8() }
    };
}
json to_json(const FundsDecimal& fd)
{
    return {
        { "uint64", fd.funds.value() },
        { "str", fd.to_string() },
        { "precision", fd.precision.value() }
    };
}
auto to_json_visit(const api::WartTransferTransaction& tx)
{
    json j;
    json jtx(to_json_temporal(tx));
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["toAddress"] = tx.toAddress.to_string();
    jtx["amount"] = to_json(tx.amount);
    jtx["pinHeight"] = tx.pinHeight;
    jtx["fee"] = to_json(tx.fee);
    jtx["fromAddress"] = tx.originAddress.to_string();
    jtx["nonceId"] = tx.nonceId;
    j["transaction"] = jtx;
    j["type"] = "Transfer";
    return j;
}
auto to_json_visit(const api::RewardTransaction& tx)
{
    json j;
    json jtx(to_json_temporal(tx));
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["toAddress"] = tx.toAddress.to_string();
    jtx["amount"] = to_json(tx.wart);
    j["type"] = "Reward";
    j["transaction"] = jtx;
    return j;
}
auto to_json_visit(const api::TokenTransferTransaction& tx)
{
    // NonceId nonceId;
    // PinHeight pinHeight;
    // Address toAddress;
    // Funds_uint64 amount;
    // AssetIdHashNamePrecision assetInfo;
    json j;
    json jtx(to_json_temporal(tx));
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["fromAddresss"] = tx.originAddress.to_string();
    jtx["fee"] = to_json(tx.fee);
    jtx["nonceId"] = tx.nonceId;
    jtx["pinHeight"] = tx.pinHeight.value(),
    jtx["toAddress"] = tx.toAddress.to_string();
    jtx["amount"] = to_json(tx.amount);
    jtx["token"] = to_json(tx.assetInfo);
    j["type"] = "TokenTransfer";
    j["transaction"] = jtx;
    return j;
}
auto to_json_visit(const api::AssetCreationTransaction& tx)
{
    // TxHash txhash;
    // AssetName assetName;
    // FundsDecimal supply;
    // std::optional<AssetId> assetId;

    json jtx(to_json_temporal(tx));
    json j;
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["toAddress"] = tx.originAddress.to_string();
    jtx["confirmations"] = tx.confirmations;
    jtx["blockHeight"] = tx.height;
    jtx["
    jtx["supply"] = to_json(tx.supply.to_string());
    jtx["timestamp"] = tx.timestamp;
    jtx["utc"] = format_utc(tx.timestamp);
    jtx["type"] = "Reward";
    j["transaction"] = jtx;
    return j;
}
auto to_json_visit(const api::NewOrderTransaction& tx)
{
    json j;
    json jtx(to_json_temporal(tx));
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["originAddress"] = tx.originAddress.to_string();
    jtx["confirmations"] = tx.confirmations;
    jtx["blockHeight"] = tx.height;
    jtx["amount"] = to_json(tx.amount);
    jtx["timestamp"] = tx.timestamp;
    jtx["utc"] = format_utc(tx.timestamp);
    jtx["type"] = "Reward";
    j["transaction"] = jtx;
    return j;
}
auto to_json_visit(const api::MatchTransaction& tx)
{
    json j;
    json jtx(to_json_temporal(tx));
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["address"] = tx.to_string();
    jtx["confirmations"] = tx.confirmations;
    jtx["blockHeight"] = tx.height;
    jtx["amount"] = to_json(tx.amount);
    jtx["timestamp"] = tx.timestamp;
    jtx["utc"] = format_utc(tx.timestamp);
    jtx["type"] = "Reward";
    j["transaction"] = jtx;
    return j;
}
auto to_json_visit(const api::LiquidityDepositTransaction& tx)
{
    json j;
    json jtx(to_json_temporal(tx));
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["toAddress"] = tx.toAddress.to_string();
    jtx["confirmations"] = tx.confirmations;
    jtx["blockHeight"] = tx.height;
    jtx["amount"] = to_json(tx.amount);
    jtx["timestamp"] = tx.timestamp;
    jtx["utc"] = format_utc(tx.timestamp);
    jtx["type"] = "Reward";
    j["transaction"] = jtx;
    return j;
}
auto to_json_visit(const api::LiquidityWithdrawalTransaction& tx)
{
    json j;
    json jtx;
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["toAddress"] = tx.toAddress.to_string();
    jtx["confirmations"] = tx.confirmations;
    jtx["blockHeight"] = tx.height;
    jtx["amount"] = to_json(tx.amount);
    jtx["timestamp"] = tx.timestamp;
    jtx["utc"] = format_utc(tx.timestamp);
    jtx["type"] = "Reward";
    j["transaction"] = jtx;
    return j;
}
auto to_json_visit(const api::CancelationTransaction& tx)
{
    json j;
    json jtx;
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["toAddress"] = tx.toAddress.to_string();
    jtx["confirmations"] = tx.confirmations;
    jtx["blockHeight"] = tx.height;
    jtx["amount"] = to_json(tx.amount);
    jtx["timestamp"] = tx.timestamp;
    jtx["utc"] = format_utc(tx.timestamp);
    jtx["type"] = "Reward";
    j["transaction"] = jtx;
    return j;
}
,

    json to_json(const PeerDB::BanEntry& item)
{
    return {
        { "ip", item.ip.to_string().c_str() },
        { "expires", item.banuntil },
        { "reasion", item.offense.err_name() },
    };
}

namespace {
json to_json(const api::ThrottleState::BatchThrottler& bt)
{
    return {
        { "h1", bt.h0.value() },
        { "h2", bt.h1.value() },
        { "window", bt.window }
    };
}
}

json to_json(const api::ThrottleState& ts)
{
    using namespace std::chrono;

    return {
        { "delay", duration_cast<seconds>(ts.delay).count() },
        { "blockRequest", to_json(ts.blockreq) },
        { "headerRequest", to_json(ts.batchreq) }
    };
}

json to_json(const Hash& h)
{
    json j;
    j["hash"] = serialize_hex(h);
    return j;
}

json to_json(const TxHash& h)
{
    json j;
    j["txHash"] = serialize_hex(h);
    return j;
}

json to_json(const api::ChainHead& ch)
{
    return {
        { "hash", serialize_hex(ch.hash) },
        { "height", ch.height },
        { "difficulty", ch.nextTarget.difficulty() },
        { "is_janushash", ch.nextTarget.is_janushash() },
        { "pinHeight", ch.pinHeight },
        { "worksum", ch.worksum.getdouble() },
        { "worksumHex", ch.worksum.to_string() },
        { "pinHash", serialize_hex(ch.pinHash) },
        { "hashrate", ch.hashrate },
    };
}

json to_json(const api::Head& h)
{
    auto j(to_json(h.chainHead));
    j["synced"] = h.synced;
    return j;
}

json to_json(const std::pair<NonzeroHeight, Header>& h)
{
    return json {
        { "header", header_json(h.second, h.first) }
    };
}

json to_json(const api::TransmissionTimeseries& tt)
{
    json arr;
    for (auto& [host, ts] : tt.byHost) {
        json tsjson(json::array());
        for (auto& e : ts) {
            tsjson.push_back({
                { "begin", e.begin.value() },
                { "end", e.end.value() },
                { "rx", e.rx },
                { "tx", e.tx },
            });
        }
        arr[host] = tsjson;
    }
    return arr;
}

json to_json(const api::MiningState& ms)
{
    auto& mt { ms.miningTask };
    json j;
    auto height { mt.block.height };
    auto blockReward { mt.block.body.reward };
    j["synced"] = ms.synced;
    j["header"] = serialize_hex(mt.block.header);
    j["difficulty"] = mt.block.header.target(height, is_testnet()).difficulty();
    j["merklePrefix"] = serialize_hex(mt.block.body.merkleLeaves.merkle_prefix());
    j["body"] = serialize_hex(mt.block.body.data);
    j["blockReward"] = to_json(blockReward.wart());
    j["height"] = height;
    j["testnet"] = is_testnet();
    return j;
}
json to_json(const api::MempoolEntries& entries)
{
    json j;
    json a = json::array();
    for (auto& e : entries.entries) {
        json elem;
        elem["fromAddress"] = e.from_address(e.txHash).to_string();
        elem["pinHeight"] = e.pin_height();
        elem["txHash"] = serialize_hex(e.txHash);
        elem["nonceId"] = e.nonce_id();
        elem["fee"] = to_json(e.fee());
        elem["toAddress"] = e.to_addr().to_string();
        elem["amount"] = to_json(e.wart());
        a.push_back(elem);
    }
    j["data"] = a;
    return j;
}

json to_json(const api::Transaction& tx)
{
    return std::visit([&](const auto& e) {
        return to_json_visit(e);
    },
        tx);
}

json to_json(const Peeraddr& ea)
{
    return ea.to_string();
}

json to_json(const api::PeerinfoConnections& pc)
{
    return to_json(pc.v, pc.map);
}

json to_json(const api::TransactionsByBlocks& txs)
{
    json arr = json::array();
    for (auto iter = txs.blocks_reversed.begin(); iter != txs.blocks_reversed.end(); ++iter) {
        auto& block { *iter };
        arr.push_back(
            json {
                { "header", header_json(block.header, block.height) },
                { "body", body_json(block) },
                { "timestamp", block.header.timestamp() },
                { "utc", format_utc(block.header.timestamp()) },
                { "confirmations", block.confirmations },
                { "height", block.height } });
    }
    return json {
        { "fromId", txs.fromId },
        { "count", txs.count },
        { "perBlock", arr }
    };

    return arr;
}

json to_json(const api::Block& block)
{
    json j;
    HeaderView hv(block.header.data());
    j["header"] = header_json(block.header, block.height);
    j["body"] = body_json(block);
    j["timestamp"] = hv.timestamp();
    j["utc"] = format_utc(hv.timestamp());
    j["confirmations"] = block.confirmations;
    j["height"] = block.height;
    return j;
}
json to_json(const api::BlockSummary& block)
{
    json j;
    HeaderView hv(block.header.data());
    j["header"] = header_json(block.header, block.height);
    j["confirmations"] = block.confirmations;
    j["height"] = block.height;
    j["nTransfers"] = block.nTransfers;
    j["miner"] = block.miner.to_string();
    j["transferred"] = to_json(block.transferred);
    j["totalTxFee"] = to_json(block.totalTxFee);
    auto r { block.height.reward() };
    j["blockReward"] = to_json(r);
    return j;
}

json to_json(const api::AccountHistory& h)
{
    json a = json::array();
    auto& reversed = h.blocks_reversed;
    for (size_t i = 0; i < reversed.size(); ++i) {
        auto& b = reversed[reversed.size() - 1 - i];
        json elem;
        elem["height"] = b.height;
        elem["confirmations"] = b.confirmations;
        elem["transactions"] = body_json(b);
        a.push_back(elem);
    }
    json j;
    j["perBlock"] = a;
    j["fromId"] = h.fromId;
    j["balance"] = to_json(h.balance);
    return j;
}

json to_json(const api::AddressCount& ac)
{
    return {
        { "address", ac.address.to_string() },
        { "count", ac.count }
    };
}

json to_json(const api::Richlist& l)
{
    json a = json::array();
    for (auto& [address, balance] : l.entries) {
        json elem;
        elem["address"] = address.to_string();
        elem["balance"] = to_json(balance);
        a.push_back(elem);
    }
    return a;
}

nlohmann::json to_json(const api::Wallet& w)
{
    auto pubKey { w.pk.pubkey() };
    return {
        { "privKey", w.pk.to_string() },
        { "pubKey", pubKey.to_string() },
        { "address", pubKey.address().to_string() }
    };
}

json to_json(const api::HashrateInfo& hi)
{
    return json {
        { "lastNBlocksEstimate", hi.estimate },
        { "N", hi.nBlocks }
    };
}

json to_json(const api::HashrateBlockChart& c)
{
    json data(json::array());
    for (const auto& v : c.chart) {
        data.push_back(v);
    }
    return json {
        { "range", json { { "min", c.range.begin }, { "max", c.range.end } } },
        { "data", data }
    };
}
json to_json(const api::HashrateTimeChart& c)
{
    json data(json::array());
    for (const auto& [timestamp, height, hashrate] : std::ranges::reverse_view(c.chartReversed)) {
        data.push_back({ { "timestamp", timestamp },
            { "height", height.value() },
            { "hashrate", hashrate } });
    }
    return json {
        { "chart", data },
        { "begin", c.begin },
        { "end", c.end },
        { "interval", c.interval },
    };
}

json to_json(const OffenseEntry& e)
{
    return json {
        { "ip", e.ip.to_string().c_str() },
        { "timestamp", e.timestamp },
        { "utc", format_utc(e.timestamp) },
        { "offense", e.offense.err_name() }
    };
}

json to_json(const api::ThrottledPeer& pi)
{
    return {
        { "throttle", to_json(pi.throttle) },
        { "connection",
            json {
                { "endpoint", pi.endpoint.to_string() },
                { "id", pi.id },
            } },
    };
}
json to_json(const api::Peerinfo& pi)
{
    json elem;
    auto conn = json {
        { "port", pi.endpoint.port() },
        { "sinceTimestamp", pi.since },
        { "sinceUTC", format_utc(pi.since) }
    };
    if (auto ip { pi.endpoint.ip() }; ip.has_value())
        conn["ip"] = ip->to_string();
    else
        conn["ip"] = nullptr;

    elem["throttle"] = to_json(pi.throttle);
    elem["connection"] = conn;

    elem["leaderPriority"] = json {
        { "ack", json { { "importance", pi.acknowledgedSnapshotPriority.importance }, { "height", pi.acknowledgedSnapshotPriority.height } } },
        { "theirs", json { { "importance", pi.theirSnapshotPriority.importance }, { "height", pi.theirSnapshotPriority.height } } }
    };
    elem["chain"] = json {
        { "length", pi.chainstate.descripted()->chain_length() },
        { "forkLower", pi.chainstate.consensus_fork_range().lower() },
        { "forkUpper", pi.chainstate.consensus_fork_range().upper() },
        { "descriptor", pi.chainstate.descripted()->descriptor },
        { "worksum", pi.chainstate.descripted()->worksum().getdouble() },
        { "worksumHex", pi.chainstate.descripted()->worksum().to_string() },
        { "grid", grid_json(pi.chainstate.descripted()->grid()) }
    };
    return elem;
}

json to_json(const TCPPeeraddr& a)
{
    return a.to_string();
}

json to_json(const api::WartBalance& b)
{
    json j;
    j["balance"] = to_json(b.balance);
    if (b.address) {
        j["address"] = b.address->address.to_string();
        j["accountId"] = b.address->accountId.value();
    } else {
        j["address"] = nullptr;
        j["accountId"] = nullptr;
    }
    return j;
}

json to_json(const Grid& g)
{
    json j(json::array());
    for (const auto& h : g) {
        j.push_back(serialize_hex(h));
    }
    return j;
}

json to_json(const std::optional<SignedSnapshot>& sp)
{
    if (sp) {
        auto& s = *sp;
        return json {
            { "priority", json { { "height", s.priority.height }, { "importance", s.priority.importance } } },
            { "hash", serialize_hex(s.hash) },
            { "signature", s.signature.to_string() },
        };
    }
    return nullptr;
}

json to_json(const TransactionId& txid)
{
    return json {
        { "accountId", txid.accountId },
        { "nonceId", txid.nonceId },
        { "pinHeight", txid.pinHeight },
    };
}

json to_json(const AssetBasic& t)
{
    return {
        { "hash", serialize_hex(t.hash) },
        { "id", t.id.value() },
        { "name", t.name.to_string() }
    };
}

json to_json(const chainserver::TransactionIds& txids)
{
    json j(json::array());
    for (auto& txid : txids) {
        j.push_back(json {
            { "accountId", txid.accountId },
            { "nonceId", txid.nonceId },
            { "pinHeight", txid.pinHeight },
        });
    }
    return j;
}

nlohmann::json to_json(const api::Round16Bit& e)
{
    auto c { CompactUInt::compact(e.original) };
    return json {
        { "original", to_json(e.original) },
        { "rounded", to_json(c.uncompact()) },
        { "16bit", c.value() }
    };
}

nlohmann::json to_json(const PrintNodeVersion&)
{
    return json {
        { "name", CMDLINE_PARSER_VERSION },
        { "major", VERSION_MAJOR },
        { "minor", VERSION_MINOR },
        { "patch", VERSION_PATCH },
        { "commit", GIT_COMMIT_INFO }
    };
}

nlohmann::json to_json(const api::Rollback& rb)
{
    return json {
        { "length", rb.length }
    };
}

nlohmann::json to_json(const api::IPCounter& ipc)
{
    json obj(json::object());
    for (auto& [ip, c] : ipc.vector) {
        obj.push_back({ ip.to_string(), c });
    }
    return obj;
}

nlohmann::json to_json(const api::NodeInfo& info)
{
    using namespace std;
    using namespace std::chrono;
    using namespace std::string_literals;
    using sc = std::chrono::steady_clock;
    auto format_duration = [](size_t s /* uptime seconds*/) {
        size_t days = s / (24 * 60 * 60);
        size_t hours = (s % (24 * 60 * 60)) / (60 * 60);
        size_t minutes = (s % (60 * 60)) / 60;
        size_t seconds = (s % 60);
        return to_string(days) + "d "s + to_string(hours) + "h "s + to_string(minutes) + "m "s + to_string(seconds) + "s"s;
    };

    auto startedAt { config().started_at() };
    auto uptime { sc::now() - startedAt.steady };
    auto uptimeSeconds(duration_cast<seconds>(uptime).count());
    auto uptimeStr { format_duration(uptimeSeconds) };
    uint32_t sinceTimestamp(duration_cast<seconds>(startedAt.system.time_since_epoch()).count());
    return {
        { "dbSize", info.dbSize },
        { "chainDBPath", config().data.chaindb },
        { "peersDBPath", config().data.peersdb },
        { "rxtxDBPath", config().data.rxtxdb },
        { "version", { { "name", CMDLINE_PARSER_VERSION }, { "major", VERSION_MAJOR }, { "minor", VERSION_MINOR }, { "patch", VERSION_PATCH }, { "commit", GIT_COMMIT_INFO } } },
        { "uptime", { { "sinceTimestamp", sinceTimestamp }, { "sinceUTC", format_utc(sinceTimestamp) }, { "seconds", uptimeSeconds }, { "formatted", uptimeStr }

                    } }
    };
}

std::string serialize(const api::Raw& r)
{
    return r.s;
}

// std::string endpoints(const Eventloop& e)
// {
//     auto [verified, failed, unverified, pending] = Inspector::endoints(e);
//     json j;
//     j["verified"] = verified_json(*verified);
//     j["failed"] = endpoint_json(*failed);
//     j["unverified"] = endpoint_json(*unverified);
//     auto a = *pending;
//     j["pending"] = pending_json(*pending);
//     return j.dump(1);
// }

// std::string connect_timers(const Eventloop& e)
// {
//     return Inspector::endpoint_timers(e);
// }
std::string header_download(const Eventloop& e)
{
    return Inspector::header_download(e);
}

// SubscriptionAction parse_subscribe_throw(std::string_view s)
// {
//     auto j { json::parse(s) };
//     if (j.size() != 1 || !j.is_object())
//         goto failed;
//
//     if (auto iter { j.find("subscribe") }; iter != j.end()) {
//         return { true, iter.value().get<std::string>() };
//     }
//     if (auto iter { j.find("unsubscribe") }; iter != j.end()) {
//         return { false, iter.value().get<std::string>() };
//     }
// failed:
//     throw std::runtime_error("Cannot parse subscription " + std::string(s));
// };

} // namespace jsonmsg
