#pragma once
#include "block/body/container.hpp"
#include "block/body/primitives.hpp"
#include "block/body/transaction_id.hpp"
#include "block/chain/range.hpp"
#include "block/chain/signed_snapshot.hpp"
#include "block/header/header.hpp"
#include "block/header/shared_batch.hpp"
#include "crypto/hash.hpp"
#include "general/descriptor.hpp"
#include "general/params.hpp"
#include "general/tcp_util.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

class Reader;
class Sndbuffer;
class ConsensusSlave;
namespace mempool {
struct EntryValue;
using Entry = std::pair<TransactionId, EntryValue>;
}

struct WithNonce {
    uint32_t nonce;
};
struct RandNonce : public WithNonce {
    RandNonce();
    RandNonce(uint32_t nonce)
        : WithNonce { nonce } { };
};

namespace {
struct MessageWriter;
}
template <uint8_t M>
struct MsgCode {
    static constexpr uint8_t msgcode { M };
    static MessageWriter gen_msg(size_t len);
};

struct InitMsg : public MsgCode<0> {
    InitMsg(Reader& r);
    static constexpr size_t maxSize = 100000;
    static Sndbuffer serialize_chainstate(const ConsensusSlave&);

    Descriptor descriptor;
    SignedSnapshot::Priority sp;
    Height chainLength;
    Worksum worksum;
    Grid grid;
};

struct ForkMsg : public MsgCode<1> {
    static constexpr size_t maxSize = 20000;
    static auto from_reader(Reader& r) -> ForkMsg;
    ForkMsg(Descriptor descriptor, NonzeroHeight chainLength, Worksum worksum, NonzeroHeight forkHeight, Grid grid);
    operator Sndbuffer() const;

    Descriptor descriptor;
    NonzeroHeight chainLength;
    Worksum worksum;
    NonzeroHeight forkHeight;
    Grid grid;
};

struct AppendMsg : public MsgCode<2> {
    static constexpr size_t maxSize = 4 + 4 + 32 + 80 * 100;
    static AppendMsg from_reader(Reader& r);
    AppendMsg(
        NonzeroHeight newLength,
        Worksum worksum,
        Grid grid);
    operator Sndbuffer() const;

    NonzeroHeight newLength;
    Worksum worksum;
    Grid grid;
};

struct SignedPinRollbackMsg : public MsgCode<3> {
    static SignedPinRollbackMsg from_reader(Reader& r);
    SignedPinRollbackMsg(
        SignedSnapshot signedPin,
        Height newLength,
        Worksum worksum,
        Descriptor descriptor)
        : signedSnapshot(signedPin)
        , shrinkLength(newLength)
        , worksum(std::move(worksum))
        , descriptor(descriptor) { };
    operator Sndbuffer() const;

    SignedSnapshot signedSnapshot;
    Height shrinkLength { 0 };
    Worksum worksum;
    Descriptor descriptor;
    static constexpr size_t maxSize = SignedSnapshot::binary_size + 4 + 32 + 4;
};

struct PingMsg : public RandNonce, public MsgCode<4> {
    static constexpr size_t maxSize = 30; // actually 14 but be generous to avoid bugs;
    PingMsg(SignedSnapshot::Priority sp, uint16_t maxAddresses = 5, uint16_t maxTransactions = 100)
        : sp(sp)
        , maxAddresses(maxAddresses)
        , maxTransactions(maxTransactions) { };

    PingMsg(uint32_t nonce, SignedSnapshot::Priority sp, uint16_t maxAddresses, uint16_t maxTransactions)
        : RandNonce(nonce)
        , sp(sp)
        , maxAddresses(maxAddresses)
        , maxTransactions(maxTransactions) { };
    static PingMsg from_reader(Reader& r);
    SignedSnapshot::Priority sp;
    uint16_t maxAddresses;
    uint16_t maxTransactions;
    operator Sndbuffer() const;
};

struct PongMsg : public WithNonce, public MsgCode<5> {
    static constexpr size_t maxSize = 4 + 2 + 6 * 100 + 18 * 1000;
    static PongMsg from_reader(Reader& r);
    PongMsg(uint32_t nonce, std::vector<EndpointAddress> addresses, std::vector<TxidWithFee> txids)
        : WithNonce { nonce }
        , addresses(addresses)
        , txids(std::move(txids)) { };
    operator Sndbuffer() const;

    Error check(const PingMsg&) const;
    std::vector<EndpointAddress> addresses;
    std::vector<TxidWithFee> txids;
};

struct BatchSelector {
    Descriptor descriptor;
    NonzeroHeight startHeight;
    NonzeroHeight end() const { return startHeight + length; }
    HeaderRange header_range() const { return { startHeight, end() }; };
    uint16_t length;
    BatchSelector(Descriptor d, NonzeroHeight s, uint16_t l)
        : descriptor(d)
        , startHeight(s)
        , length(l) { };
    BatchSelector(Reader& r);
};

struct BatchreqMsg : public RandNonce, public MsgCode<6> {
    static constexpr size_t maxSize = 14;
    std::string log_str() const;
    static BatchreqMsg from_reader(Reader& r);
    BatchreqMsg(BatchSelector selector)
        : selector(selector) { };
    BatchreqMsg(uint32_t nonce, BatchSelector selector)
        : RandNonce(nonce)
        , selector(selector) { };
    operator Sndbuffer() const;
    BatchSelector selector;
};

struct BatchrepMsg : public WithNonce, public MsgCode<7> {
    static constexpr size_t maxSize = 4 + HEADERBATCHSIZE * 80;
    BatchrepMsg(Reader& r);
    BatchrepMsg(uint32_t nonce, Batch b)
        : WithNonce { nonce }
        , batch(std::move(b))
    {
    }
    static BatchrepMsg from_reader(Reader& r);
    operator Sndbuffer() const;

    Batch batch;
};

struct ProbereqMsg : public RandNonce, public MsgCode<8> {
    static constexpr size_t maxSize = 12;
    std::string log_str() const;
    static ProbereqMsg from_reader(Reader& r);
    ProbereqMsg(Descriptor descriptor, NonzeroHeight height)
        : descriptor(descriptor)
        , height(height)
    {
    }
    operator Sndbuffer() const;
    Descriptor descriptor;
    NonzeroHeight height;

private:
    ProbereqMsg(uint32_t nonce, Descriptor descriptor, NonzeroHeight height)
        : RandNonce(nonce)
        , descriptor(descriptor)
        , height(height)
    {
    }
};

struct ProberepMsg : public WithNonce, public MsgCode<9> {
    static constexpr size_t maxSize = 189;
    static ProberepMsg from_reader(Reader& r);
    ProberepMsg(uint32_t nonce, uint32_t currentDescriptor)
        : WithNonce { nonce }
        , currentDescriptor(currentDescriptor) { };
    ProberepMsg(uint32_t nonce, uint32_t currentDescriptor,
        std::optional<Header> req,
        std::optional<Header> cur)
        : WithNonce { nonce }
        , currentDescriptor(currentDescriptor)
        , requested(std::move(req))
        , current(std::move(cur)) { };
    operator Sndbuffer() const;
    Descriptor currentDescriptor;
    std::optional<Header> requested;
    std::optional<Header> current;
};

struct BlockreqMsg : public RandNonce, public MsgCode<10> {
    static constexpr size_t maxSize = 48;

    // methods
    std::string log_str() const;
    BlockreqMsg(DescriptedBlockRange range)
        : range(range) { };
    BlockreqMsg(uint32_t nonce, DescriptedBlockRange range)
        : RandNonce(nonce)
        , range(range) { };
    static BlockreqMsg from_reader(Reader& r);
    operator Sndbuffer() const;

    // data
    DescriptedBlockRange range;
};

struct BlockrepMsg : public WithNonce, public MsgCode<11> {
    static constexpr size_t maxSize = MAXBLOCKBATCHSIZE * (4 + MAXBLOCKSIZE);

    // methods
    static BlockrepMsg from_reader(Reader& r);
    BlockrepMsg(uint32_t nonce, std::vector<BodyContainer> b)
        : WithNonce { nonce }
        , blocks(std::move(b)) { };
    operator Sndbuffer() const;
    bool empty() const { return blocks.empty(); }

    // data
    std::vector<BodyContainer> blocks;
};

struct TxsubscribeMsg : public RandNonce, public MsgCode<12> {
    TxsubscribeMsg(Height upper)
        : upper(upper) { };
    TxsubscribeMsg(uint32_t nonce, Height upper)
        : RandNonce(nonce)
        , upper(upper) { };
    static TxsubscribeMsg from_reader(Reader& r);
    operator Sndbuffer() const;
    Height upper;
    static constexpr size_t maxSize = 8;
};

struct TxnotifyMsg : public RandNonce, public MsgCode<13> {
    static constexpr size_t MAXENTRIES = 5000;
    TxnotifyMsg(std::vector<TxidWithFee> txids)
        : txids(std::move(txids)) { };
    TxnotifyMsg(uint32_t nonce, std::vector<TxidWithFee> txids)
        : RandNonce(nonce)
        , txids(std::move(txids)) { };
    static TxnotifyMsg from_reader(Reader& r);

    using send_iter = std::vector<mempool::Entry>::iterator;
    static Sndbuffer direct_send(send_iter begin, send_iter end);
    operator Sndbuffer() const;
    std::vector<TxidWithFee> txids;
    static constexpr size_t maxSize = 4 + TxnotifyMsg::MAXENTRIES * TransactionId::bytesize;
};

struct TxreqMsg : public RandNonce, public MsgCode<14> {
    static constexpr size_t MAXENTRIES = 5000;
    TxreqMsg(std::vector<TransactionId> txids)
        : txids(std::move(txids)) { };
    TxreqMsg(uint32_t nonce, std::vector<TransactionId> txids)
        : RandNonce(nonce)
        , txids(std::move(txids)) { };
    static TxreqMsg from_reader(Reader& r);
    operator Sndbuffer() const;
    std::vector<TransactionId> txids;
    static constexpr size_t maxSize = 2 + 4 + TxreqMsg::MAXENTRIES * TransactionId::bytesize;
};

struct TxrepMsg : public RandNonce, public MsgCode<15> {
    TxrepMsg(std::vector<std::optional<TransferTxExchangeMessage>> txs)
        : txs(txs) { };
    TxrepMsg(uint32_t nonce, std::vector<std::optional<TransferTxExchangeMessage>> txs)
        : RandNonce(nonce)
        , txs(txs) { };
    static TxrepMsg from_reader(Reader& r);
    std::vector<std::optional<TransferTxExchangeMessage>> txs;
    operator Sndbuffer() const;
    static constexpr size_t maxSize = 2 + 4 + TxreqMsg::MAXENTRIES * (1 + TransferTxExchangeMessage::bytesize);
};

struct LeaderMsg : public MsgCode<16> {
    static constexpr size_t maxSize = 4 + 32 + 65;
    static LeaderMsg from_reader(Reader& r);
    LeaderMsg(SignedSnapshot snapshot)
        : signedSnapshot(std::move(snapshot)) { };
    operator Sndbuffer() const;
    ///////
    // data
    SignedSnapshot signedSnapshot;
};

namespace messages {
[[nodiscard]] size_t size_bound(uint8_t msgtype);

using Msg = std::variant<InitMsg, ForkMsg, AppendMsg, SignedPinRollbackMsg, PingMsg, PongMsg, BatchreqMsg, BatchrepMsg, ProbereqMsg, ProberepMsg, BlockreqMsg, BlockrepMsg, TxnotifyMsg, TxreqMsg, TxrepMsg, LeaderMsg>;
} // namespace messages
