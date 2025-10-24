#include "json.hpp"
#include "api/types/all.hpp"
#include "block/body/parse.hpp"
#include "block/header/header_impl.hpp"
#include "block/header/view.hpp"
#include "chainserver/transaction_ids.hpp"
#include "communication/mining_task.hpp"
#include "crypto/crypto.hpp"
#include "eventloop/eventloop.hpp"
#include "eventloop/sync/header_download/header_download.hpp"
#include "eventloop/sync/sync.hpp"
#include "general/errors.hpp"
#include "general/hex.hpp"
#include "general/is_testnet.hpp"
#include "version.hpp"
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
}
struct Inspector {
    static auto endoints(const Eventloop& e)
    {
        auto& m = e.connections;
        return std::tuple { &m.verified, &m.failedAddresses.data(), &m.unverifiedAddresses, &m.pendingOutgoing };
    }
    static auto& ip_counter(const Conman& c)
    {
        return c.perIpCounter;
    }
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
    static std::string endpoint_timers(const Eventloop& c)
    {
        auto now = steady_clock::now();
        auto& m = c.connections;
        using VerIter = decltype(c.connections)::VerIter;
        using PinIter = decltype(c.connections)::PinIter;
        json j;
        json verifiers = json::array();
        json pins = json::array();
        for (auto& [t, v] : m.timer) {
            json e;
            e["expiresSeconds"] = duration_cast<seconds>(t - now).count();
            if (std::holds_alternative<VerIter>(v)) {
                auto& iter = std::get<VerIter>(v);
                auto& n = iter->second;
                e["endpoint"] = iter->first.to_string();
                e["seenSecondsAgo"] = n.outboundConnection ? 0 : duration_cast<seconds>(now - n.lastVerified).count();
                verifiers.push_back(e);
            } else {
                assert(std::holds_alternative<PinIter>(v));
                auto& iter = std::get<PinIter>(v);
                e["endpoint"] = iter->first.to_string();
                e["sleepOnFailedSeconds"] = iter->second.sleepSeconds;
                pins.push_back(e);
            }
        }
        j["verifiers"] = verifiers;
        j["pins"] = pins;
        return j.dump(1);
    }
};

namespace {
template <typename T>
json verified_json(const std::map<EndpointAddress, T>& map)
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
json pending_json(const std::map<EndpointAddress, std::chrono::steady_clock::time_point>& m)
{
    auto now = steady_clock::now();
    json e = json::array();
    for (const auto& [ae, tp] : m) {
        json j = {
            { "endpoints", ae.to_string() },
            { "seconds", duration_cast<seconds>(now - tp).count() }
        };
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
    h["version"] = serialize_hex(version);
    return h;
}

[[nodiscard]] json body_json(const API::Block& b)
{
    json out;
    { // rewards
        json a = json::array();
        for (auto& r : b.rewards) {
            json elem;
            elem["txHash"] = serialize_hex(r.txhash);
            elem["toAddress"] = r.toAddress.to_string();
            elem["amount"] = r.amount.to_string();
            elem["amountE8"] = r.amount.E8();
            a.push_back(elem);
        }
        out["rewards"] = a;
    }
    { // transfers
        json a = json::array();
        for (auto& t : b.transfers) {
            json elem;
            elem["fromAddress"] = t.fromAddress.to_string();
            elem["fee"] = t.fee.to_string();
            elem["feeE8"] = t.fee.E8();
            elem["nonceId"] = t.nonceId;
            elem["pinHeight"] = t.pinHeight;
            elem["txHash"] = serialize_hex(t.txhash);
            elem["toAddress"] = t.toAddress.to_string();
            elem["amount"] = t.amount.to_string();
            elem["amountE8"] = t.amount.E8();
            a.push_back(elem);
        }
        out["transfers"] = a;
    }
    return out;
}

} // namespace

namespace jsonmsg {
using namespace nlohmann;

auto to_json_visit(const API::TransferTransaction& tx)
{
    json j;
    json jtx;
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["toAddress"] = tx.toAddress.to_string();
    jtx["confirmations"] = tx.confirmations;
    jtx["blockHeight"] = tx.height;
    jtx["amount"] = tx.amount.to_string();
    jtx["amountE8"] = tx.amount.E8();
    jtx["type"] = "Transfer";
    jtx["pinHeight"] = tx.pinHeight;
    jtx["fee"] = tx.fee.to_string();
    jtx["feeE8"] = tx.fee.E8();
    jtx["fromAddress"] = tx.fromAddress.to_string();
    jtx["nonceId"] = tx.nonceId;
    j["transaction"] = jtx;
    return j;
}
auto to_json_visit(const API::RewardTransaction& tx)
{
    json j;
    json jtx;
    jtx["txHash"] = serialize_hex(tx.txhash);
    jtx["toAddress"] = tx.toAddress.to_string();
    jtx["confirmations"] = tx.confirmations;
    jtx["blockHeight"] = tx.height;
    jtx["amount"] = tx.amount.to_string();
    jtx["amountE8"] = tx.amount.E8();
    jtx["timestamp"] = tx.timestamp;
    jtx["utc"] = format_utc(tx.timestamp);
    jtx["type"] = "Reward";
    j["transaction"] = jtx;
    return j;
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

json to_json(const API::Head& h)
{
    auto& ch = h.chainHead;
    json j;
    j["hash"] = serialize_hex(ch.hash);
    j["synced"] = h.synced;
    j["height"] = ch.height;
    j["difficulty"] = ch.nextTarget.difficulty();
    j["is_janushash"] = ch.nextTarget.is_janushash();
    j["pinHeight"] = ch.pinHeight;
    j["worksum"] = ch.worksum.getdouble();
    j["worksumHex"] = ch.worksum.to_string();
    j["pinHash"] = serialize_hex(ch.pinHash);
    return j;
}

json to_json(const std::pair<NonzeroHeight, Header>& h)
{
    return json {
        { "header", header_json(h.second, h.first) }
    };
}

json to_json(const API::MiningState& ms)
{
    auto& mt { ms.miningTask };
    json j;
    auto height { mt.block.height };
    auto bodyView { mt.block.body_view() };
    auto blockReward { bodyView.reward() };
    auto totalTxFee { bodyView.fee_sum_assert() };
    j["synced"] = ms.synced;
    j["header"] = serialize_hex(mt.block.header);
    j["difficulty"] = mt.block.header.target(height, is_testnet()).difficulty();
    j["merklePrefix"] = serialize_hex(bodyView.merkle_prefix());
    j["body"] = serialize_hex(mt.block.body.data());
    j["blockReward"] = blockReward.amount_assert().to_string();
    j["blockRewardE8"] = blockReward.amount_assert().E8();
    j["totalTxFee"] = totalTxFee.to_string();
    j["totalTxFeeE8"] = totalTxFee.E8();
    j["height"] = height;
    j["testnet"] = is_testnet();
    return j;
}

json to_json(const API::MempoolUpdate& r)
{
    return {
        { "deleted", r.deletedTransactions }
    };
}

json to_json(const API::MempoolEntries& entries)
{
    json j;
    json a = json::array();
    for (auto& e : entries.entries) {
        json elem;
        elem["fromAddress"] = e.from_address(e.txHash).to_string();
        elem["pinHeight"] = e.pin_height();
        elem["txHash"] = serialize_hex(e.txHash);
        elem["nonceId"] = e.nonce_id();
        elem["fee"] = e.fee().to_string();
        elem["feeE8"] = e.fee().E8();
        elem["toAddress"] = e.toAddr.to_string();
        elem["amount"] = e.amount.to_string();
        elem["amountE8"] = e.amount.E8();
        a.push_back(elem);
    }
    j["data"] = a;
    return j;
}

json to_json(const API::Transaction& tx)
{
    return std::visit([&](const auto& e) {
        return to_json_visit(e);
    },
        tx);
}

json to_json(const EndpointAddress& ea)
{
    return ea.to_string();
}

json to_json(const API::PeerinfoConnections& pc)
{
    return to_json(pc.v, pc.map);
};

json to_json(const API::TransactionsByBlocks& txs)
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

json to_json(const API::TransactionMinfee& f)
{
    return {
        { "amount", f.minfee.to_string() },
        { "E8", f.minfee.uncompact().E8() },
        { "16bit", f.minfee.value() }
    };
}

json to_json(const API::Block& block)
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

json to_json(const API::AccountHistory& h)
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
    j["balance"] = h.balance.to_string();
    j["balanceE8"] = h.balance.E8();
    return j;
}

json to_json(const API::Richlist& l)
{
    json a = json::array();
    for (auto& [address, balance] : l.entries) {
        json elem;
        elem["address"] = address.to_string();
        elem["balance"] = balance.to_string();
        elem["balanceE8"] = balance.E8();
        a.push_back(elem);
    }
    return a;
}

nlohmann::json to_json(const API::Wallet& w)
{
    auto pubKey { w.pk.pubkey() };
    return {
        { "privKey", w.pk.to_string() },
        { "pubKey", pubKey.to_string() },
        { "address", pubKey.address().to_string() }
    };
}

json to_json(const API::HashrateInfo& hi)
{
    return json {
        { "lastNBlocksEstimate", hi.estimate },
        { "N", hi.nBlocks }
    };
}

json to_json(const API::HashrateChart& c)
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

json to_json(const OffenseEntry& e)
{
    return json {
        { "ip", e.ip.to_string().c_str() },
        { "timestamp", e.timestamp },
        { "utc", format_utc(e.timestamp) },
        { "offense", e.offense.err_name() }
    };
}

std::string serialize(const std::vector<API::Peerinfo>& connected)
{
    using namespace nlohmann;
    json j = json::array();
    for (auto& item : connected) {
        json elem;
        elem["connection"] = json {
            { "ip", item.endpoint.ipv4.to_string().c_str() },
            { "port", item.endpoint.port },
            { "sinceTimestamp", item.since },
            { "sinceUtc", format_utc(item.since) }
        };
        elem["leaderPriority"] = json {
            { "ack", json { { "importance", item.acknowledgedSnapshotPriority.importance }, { "height", item.acknowledgedSnapshotPriority.height } } },
            { "theirs", json { { "importance", item.theirSnapshotPriority.importance }, { "height", item.theirSnapshotPriority.height } } }
        };
        elem["chain"] = json {
            { "length", item.chainstate.descripted()->chain_length() },
            { "forkLower", item.chainstate.consensus_fork_range().lower() },
            { "forkUpper", item.chainstate.consensus_fork_range().upper() },
            { "descriptor", item.chainstate.descripted()->descriptor },
            { "worksum", item.chainstate.descripted()->worksum().getdouble() },
            { "worksumHex", item.chainstate.descripted()->worksum().to_string() },
            { "grid", grid_json(item.chainstate.descripted()->grid()) }
        };
        j.push_back(elem);
    }
    return j.dump(1);
}

json to_json(const API::Balance& b)
{
    json j;
    j["balance"] = b.balance.to_string();
    j["balanceE8"] = b.balance.E8();
    if (b.address)
        j["address"] = b.address->to_string();
    else
        j["address"] = nullptr;
    auto v { b.accountId.value() };
    if (v > 0)
        j["accountId"] = v;
    else
        j["accountId"] = nullptr;
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

nlohmann::json to_json(const chainserver::TransactionIds& txids)
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

nlohmann::json to_json(const API::Round16Bit& e)
{
    auto c { CompactUInt::compact(e.original) };
    return json {
        { "originalE8", e.original.E8() },
        { "originalAmount", e.original.to_string() },
        { "roundedE8", c.uncompact().E8() },
        { "roundedAmount", c.uncompact().to_string() },
        { "16bit", c.value() }
    };
};

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

nlohmann::json to_json(const API::Rollback& rb)
{
    return json {
        { "length", rb.length }
    };
}

std::string serialize(const API::Raw& r)
{
    return r.s;
}

std::string endpoints(const Eventloop& e)
{
    auto [verified, failed, unverified, pending] = Inspector::endoints(e);
    json j;
    j["verified"] = verified_json(*verified);
    j["failed"] = endpoint_json(*failed);
    j["unverified"] = endpoint_json(*unverified);
    auto a = *pending;
    j["pending"] = pending_json(*pending);
    return j.dump(1);
}

std::string connect_timers(const Eventloop& e)
{
    return Inspector::endpoint_timers(e);
}
std::string header_download(const Eventloop& e)
{
    return Inspector::header_download(e);
}

std::string ip_counter(const Conman& e)
{
    auto& ipCounter = Inspector::ip_counter(e);
    json j = json::object();
    for (auto& [ip, count] : ipCounter.data()) {
        j[ip.to_string()] = count;
    }
    return j.dump(1);
}

} // namespace jsonmsg
