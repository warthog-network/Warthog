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
#include "message_elements/helper_types.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
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
        : WithNonce { nonce } {};
};

namespace {
struct MessageWriter;
}

template <uint8_t M>
struct MsgCode {
    static constexpr uint8_t msgcode { M };
    static MessageWriter gen_msg(size_t len);
};

// template<size_t i, typename ...T>
// struct Msg;

template <size_t, typename... T>
struct MsgI;

template <size_t i, typename T>
struct MsgI<i, T> {

    template <size_t j>
    T& get()
    requires(j == j)
    {
        return t;
    }

    MsgI(T t)
        : t(std::move(t))
    {
    }

    MsgI(Reader& r)
        : MsgI(T { r })
    {
    }
    T t;
};

template <size_t i, typename T, typename... Rest>
struct MsgI<i, T, Rest...> : public MsgI<i + 1, Rest...> {
    MsgI(T t, Reader& r)
        : MsgI<i + 1, Rest...>(r)
        , t(std::move(t))
    {
    }
    MsgI(Reader& r)
        : MsgI { T(t), r }
    {
    }

    template <size_t j>
    auto& get()
    {
        if constexpr (j == i)
            return t;
        else
            return MsgI<i + 1, Rest...>::template get<j>();
    }

private:
    T t;
};

template <typename... Ts>
using MsgCombine = MsgI<0, Ts...>;

struct InitMsg2 : public MsgCombine<Descriptor, SignedSnapshot::Priority, Height, Worksum, Grid>, public MsgCode<0> {
    InitMsg2(Reader& r);
    // using Msg<Descriptor, SignedSnapshot::Priority, Height, Worksum, Grid>::Msg;
    static constexpr size_t maxSize = 100000;
    static Sndbuffer serialize_chainstate(const ConsensusSlave&);

    auto& descriptor() { return get<0>(); }
    auto& sp() { return get<1>(); }
    auto& chainLength() { return get<2>(); }
    auto& worksum() { return get<3>(); }
    auto& grid() { return get<4>(); }
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
    ForkMsg(Reader& r);
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
    AppendMsg(Reader& r);
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
    SignedPinRollbackMsg(
        SignedSnapshot signedPin,
        Height newLength,
        Worksum worksum,
        Descriptor descriptor)
        : signedSnapshot(signedPin)
        , shrinkLength(newLength)
        , worksum(std::move(worksum))
        , descriptor(descriptor) {};
    SignedPinRollbackMsg(Reader& r);
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
        , maxTransactions(maxTransactions) {};

    PingMsg(uint32_t nonce, SignedSnapshot::Priority sp, uint16_t maxAddresses, uint16_t maxTransactions)
        : RandNonce(nonce)
        , sp(sp)
        , maxAddresses(maxAddresses)
        , maxTransactions(maxTransactions) {};
    PingMsg(Reader& r);
    SignedSnapshot::Priority sp;
    uint16_t maxAddresses;
    uint16_t maxTransactions;
    operator Sndbuffer() const;
};

struct PongMsg : public WithNonce, public MsgCode<5> {
    static constexpr size_t maxSize = 4 + 2 + 6 * 100 + 18 * 1000;
    PongMsg(Reader& r);
    PongMsg(uint32_t nonce, messages::Vector16<TCPSockaddr> addresses, messages::Vector16<TxidWithFee> txids)
        : WithNonce { nonce }
        , addresses(addresses)
        , txids(std::move(txids)) {};
    operator Sndbuffer() const;

    Error check(const PingMsg&) const;
    messages::Vector16<TCPSockaddr> addresses;
    messages::Vector16<TxidWithFee> txids;
};

struct BatchreqMsg : public RandNonce, public MsgCode<6> {
    static constexpr size_t maxSize = 14;
    std::string log_str() const;
    BatchreqMsg(Reader& r);
    BatchreqMsg(BatchSelector selector)
        : selector(selector) {};
    BatchreqMsg(uint32_t nonce, BatchSelector selector)
        : RandNonce(nonce)
        , selector(selector) {};
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
    operator Sndbuffer() const;

    Batch batch;
};

struct ProbereqMsg : public RandNonce, public MsgCode<8> {
    static constexpr size_t maxSize = 12;
    std::string log_str() const;
    ProbereqMsg(Reader& r);
    ProbereqMsg(Descriptor descriptor, NonzeroHeight height)
        : descriptor(descriptor)
        , height(height)
    {
    }
    operator Sndbuffer() const;
    Descriptor descriptor;
    NonzeroHeightParser<EPROBEHEIGHT> height;

private:
    ProbereqMsg(uint32_t nonce, Descriptor descriptor, NonzeroHeightParser<EPROBEHEIGHT> height)
        : RandNonce(nonce)
        , descriptor(descriptor)
        , height(height)
    {
    }
};

struct ProberepMsg : public WithNonce, public MsgCode<9> {
    static constexpr size_t maxSize = 189;
    ProberepMsg(Reader& r);
    ProberepMsg(uint32_t nonce, uint32_t currentDescriptor)
        : WithNonce { nonce }
        , currentDescriptor(currentDescriptor) {};
    ProberepMsg(uint32_t nonce, uint32_t currentDescriptor,
        CurrentAndRequested car)
        : WithNonce { nonce }
        , currentDescriptor(currentDescriptor)
        , currentAndRequested(std::move(car))
    {
    }
    auto& current() const { return currentAndRequested.current; }
    auto& current() { return currentAndRequested.current; }
    auto& requested() { return currentAndRequested.requested; }
    auto& requested() const { return currentAndRequested.requested; }
    operator Sndbuffer() const;
    Descriptor currentDescriptor;
    CurrentAndRequested currentAndRequested;
};

struct BlockreqMsg : public RandNonce, public MsgCode<10> {
    static constexpr size_t maxSize = 48;

    // methods
    std::string log_str() const;
    BlockreqMsg(DescriptedBlockRange range)
        : range(range) {};
    BlockreqMsg(uint32_t nonce, DescriptedBlockRange range)
        : RandNonce(nonce)
        , range(range) {};
    BlockreqMsg(Reader& r);
    operator Sndbuffer() const;

    // data
    DescriptedBlockRange range;
};

struct BlockrepMsg : public WithNonce, public MsgCode<11> {
    static constexpr size_t maxSize = MAXBLOCKBATCHSIZE * (4 + MAXBLOCKSIZE);

    // methods
    BlockrepMsg(Reader& r);
    BlockrepMsg(uint32_t nonce, messages::VectorRest<BodyContainer> b)
        : WithNonce { nonce }
        , blocks(std::move(b)) {};
    operator Sndbuffer() const;
    bool empty() const { return blocks.empty(); }

    // data
    messages::VectorRest<BodyContainer> blocks;
};

struct TxsubscribeMsg : public RandNonce, public MsgCode<12> {
    TxsubscribeMsg(Height upper)
        : upper(upper) {};
    TxsubscribeMsg(uint32_t nonce, Height upper)
        : RandNonce(nonce)
        , upper(upper) {};
    TxsubscribeMsg(Reader& r);
    operator Sndbuffer() const;
    Height upper;
    static constexpr size_t maxSize = 8;
};

struct TxnotifyMsg : public RandNonce, public MsgCode<13> {
    static constexpr size_t MAXENTRIES = 5000;
    TxnotifyMsg(messages::Vector16<TxidWithFee> txids)
        : txids(std::move(txids)) {};
    TxnotifyMsg(uint32_t nonce, messages::Vector16<TxidWithFee> txids)
        : RandNonce(nonce)
        , txids(std::move(txids)) {};
    TxnotifyMsg(Reader& r);

    using send_iter = std::vector<mempool::Entry>::iterator;
    static Sndbuffer direct_send(send_iter begin, send_iter end);
    operator Sndbuffer() const;
    messages::Vector16<TxidWithFee> txids;
    static constexpr size_t maxSize = 4 + TxnotifyMsg::MAXENTRIES * TransactionId::bytesize;
};

struct TxreqMsg : public RandNonce, public MsgCode<14> {
    static constexpr size_t MAXENTRIES = 5000;
    TxreqMsg(std::vector<TransactionId> txids)
        : txids(std::move(txids)) {};
    TxreqMsg(uint32_t nonce, messages::VectorRest<TransactionId> txids)
        : RandNonce(nonce)
        , txids(std::move(txids)) {};
    TxreqMsg(Reader& r);
    operator Sndbuffer() const;
    messages::VectorRest<TransactionId> txids;
    static constexpr size_t maxSize = 2 + 4 + TxreqMsg::MAXENTRIES * TransactionId::bytesize;
};

struct TxrepMsg : public RandNonce, public MsgCode<15> {
    using vector_t = messages::VectorRest<messages::Optional<TransferTxExchangeMessage>>;
    TxrepMsg(vector_t txs)
        : txs(txs) {};
    TxrepMsg(uint32_t nonce, messages::VectorRest<messages::Optional<TransferTxExchangeMessage>> txs)
        : RandNonce(nonce)
        , txs(txs) {};
    TxrepMsg(Reader& r);
    messages::VectorRest<messages::Optional<TransferTxExchangeMessage>> txs;
    operator Sndbuffer() const;
    static constexpr size_t maxSize = 2 + 4 + TxreqMsg::MAXENTRIES * (1 + TransferTxExchangeMessage::bytesize);
};

struct LeaderMsg : public MsgCode<16> {
    static constexpr size_t maxSize = 4 + 32 + 65;
    LeaderMsg(Reader& r);
    LeaderMsg(SignedSnapshot snapshot)
        : signedSnapshot(std::move(snapshot)) {};
    operator Sndbuffer() const;
    ///////
    // data
    SignedSnapshot signedSnapshot;
};

struct PingV2Msg : public RandNonce, public MsgCode<17> {
    static constexpr size_t maxSize = 2000;
    PingV2Msg(SignedSnapshot::Priority sp, uint16_t maxAddresses = 5, uint16_t maxTransactions = 100)
        : sp(sp)
        , maxAddresses(maxAddresses)
        , maxTransactions(maxTransactions) {};

    PingV2Msg(uint32_t nonce, SignedSnapshot::Priority sp, uint16_t maxAddresses, uint16_t maxTransactions, String16 ownRTC, uint8_t maxUseOwnRTC)
        : RandNonce(nonce)
        , sp(sp)
        , maxAddresses(maxAddresses)
        , maxTransactions(maxTransactions)
        , ownRTC(std::move(ownRTC))
        , maxUseOwnRTC(maxUseOwnRTC)
    {
    }
    PingV2Msg(Reader& r);
    SignedSnapshot::Priority sp;
    uint16_t maxAddresses;
    uint16_t maxTransactions;
    String16 ownRTC;
    uint8_t maxUseOwnRTC;
    operator Sndbuffer() const;
};

struct RTCInfo : public RandNonce, public MsgCode<18> {
    static constexpr size_t maxSize = 100000;
    RTCInfo(uint32_t nonce, RTCPeers rtcPeers)
        : RandNonce(nonce)
        , rtcPeers(std::move(rtcPeers))
    {
    }

    RTCInfo(Reader& r);
    RTCPeers rtcPeers;
    operator Sndbuffer() const;
};

struct RTCSelect : public RandNonce, public MsgCode<19> {
    static constexpr size_t maxSize = 100000;
    RTCSelect(uint32_t nonce, messages::Vector8<uint32_t> selected)
        : RandNonce(nonce)
        , selected(std::move(selected))
    {
    }

    messages::Vector8<uint32_t> selected;

    RTCSelect(Reader& r);
    operator Sndbuffer() const;
};

struct RTCRequestOffer : public RandNonce, public MsgCode<20> {
    RTCRequestOffer(uint32_t nonce, uint32_t key, String16 offer)
        : RandNonce(nonce)
        , key(key)
        , offer(std::move(offer))
    {
    }
    uint32_t key;
    String16 offer;
    RTCRequestOffer(Reader& r);
    operator Sndbuffer() const;
};

struct RTCOffer : public RandNonce, public MsgCode<21> {
};

struct RTCRequestResponse : public RandNonce, public MsgCode<21> {
};

struct RTCResponse : public RandNonce, public MsgCode<22> {
};

namespace messages {
[[nodiscard]] size_t size_bound(uint8_t msgtype);

using Msg = std::variant<InitMsg, ForkMsg, AppendMsg, SignedPinRollbackMsg, PingMsg, PongMsg, BatchreqMsg, BatchrepMsg, ProbereqMsg, ProberepMsg, BlockreqMsg, BlockrepMsg, TxnotifyMsg, TxreqMsg, TxrepMsg, LeaderMsg
    // , PingV2Msg, RTCInfo
    >;
} // namespace messages
