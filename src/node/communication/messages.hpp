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
#include "message_elements/packer.hpp"
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

// template <typename... Ts>
// using MsgCombine = MsgI<0, Ts...>;

struct InitMsg2 : public MsgCombine<0, Descriptor, SignedSnapshot::Priority, Height, Worksum, Grid> {
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

struct ForkMsg : public MsgCombine<1, Descriptor, NonzeroHeight, Worksum, NonzeroHeight, messages::ReadRest<Grid>> {
    static constexpr size_t maxSize = 20000;
    using MsgCombine<1, Descriptor, NonzeroHeight, Worksum, NonzeroHeight, messages::ReadRest<Grid>>::MsgCombine;
    // ForkMsg(Reader& r);
    // ForkMsg(Descriptor descriptor, NonzeroHeight chainLength, Worksum worksum, NonzeroHeight forkHeight, messages::ReadRest<Grid> grid);
    // operator Sndbuffer() const;

    auto& descriptor() const { return get<0>(); }
    const NonzeroHeight& chainLength() const { return get<1>(); }
    const Worksum& worksum() const { return get<2>(); }
    const NonzeroHeight& forkHeight() const { return get<3>(); }
    const messages::ReadRest<Grid>& grid() const { return get<4>(); }
};

// struct AppendMsg : public MsgCode<2> {
//     static constexpr size_t maxSize = 4 + 4 + 32 + 80 * 100;
//     AppendMsg(Reader& r);
//     AppendMsg(
//         NonzeroHeight newLength,
//         Worksum worksum,
//         messages::ReadRest<Grid> grid);
//     operator Sndbuffer() const;
//
//     NonzeroHeight newLength;
//     Worksum worksum;
//     Grid grid;
// };
struct AppendMsg : public MsgCombine<2, NonzeroHeight, Worksum, messages::ReadRest<Grid>> {
    using MsgCombine<2, NonzeroHeight, Worksum, messages::ReadRest<Grid>>::MsgCombine;
    static constexpr size_t maxSize = 4 + 4 + 32 + 80 * 100;
    // AppendMsg(Reader& r);
    // AppendMsg(
    //     NonzeroHeight newLength,
    //     Worksum worksum,
    //     messages::ReadRest<Grid> grid);
    // operator Sndbuffer() const;

    auto& newLength() const { return get<0>(); };
    auto& worksum() const { return get<1>(); };
    auto& grid() const { return get<2>(); };
    // NonzeroHeight newLength;
    // Worksum worksum;
    // Grid grid;
};

struct SignedPinRollbackMsg : public MsgCombine<3, SignedSnapshot, Height, Worksum, Descriptor> {
    using MsgCombine<3, SignedSnapshot, Height, Worksum, Descriptor>::MsgCombine;
    // SignedPinRollbackMsg(
    //     SignedSnapshot signedPin,
    //     Height newLength,
    //     Worksum worksum,
    //     Descriptor descriptor)
    //     : signedSnapshot(signedPin)
    //     , shrinkLength(newLength)
    //     , worksum(std::move(worksum))
    //     , descriptor(descriptor) {};
    // SignedPinRollbackMsg(Reader& r);
    // operator Sndbuffer() const;

    const SignedSnapshot& signedSnapshot() const { return get<0>(); }
    const Height& shrinkLength() const { return get<1>(); }
    const Worksum& worksum() const { return get<2>(); }
    const Descriptor& descriptor() const { return get<3>(); }

    static constexpr size_t maxSize = SignedSnapshot::binary_size + 4 + 32 + 4;
};

struct PingMsg : public MsgCombineRequest<4, SignedSnapshot::Priority, uint16_t, uint16_t> {
    static constexpr size_t maxSize = 30; // actually 14 but be generous to avoid bugs;
    using MsgCombineRequest<4, SignedSnapshot::Priority, uint16_t, uint16_t>::MsgCombineRequest;

    PingMsg(SignedSnapshot::Priority sp, uint16_t maxAddresses = 5, uint16_t maxTransactions = 100)
        : MsgCombineRequest<4, SignedSnapshot::Priority, uint16_t, uint16_t>(sp, maxAddresses, maxTransactions)
    {
    }

    // PingMsg(uint32_t nonce, SignedSnapshot::Priority sp, uint16_t maxAddresses, uint16_t maxTransactions)
    //     : RandNonce(nonce)
    //     , sp(sp)
    //     , maxAddresses(maxAddresses)
    //     , maxTransactions(maxTransactions) {};
    // PingMsg(Reader& r);
    const SignedSnapshot::Priority& sp() const { return get<0>(); }
    const uint16_t& maxAddresses() const { return get<1>(); }
    const uint16_t& maxTransactions() const { return get<2>(); }
    // operator Sndbuffer() const;
};

struct PongMsg : public MsgCombineReply<5, messages::Vector16<TCPSockaddr>, messages::Vector16<TxidWithFee>> {
    static constexpr size_t maxSize = 4 + 2 + 6 * 100 + 18 * 1000;
    using MsgCombineReply<5, messages::Vector16<TCPSockaddr>, messages::Vector16<TxidWithFee>>::MsgCombineReply;
    PongMsg(uint32_t nonce, messages::Vector16<TCPSockaddr> addresses, messages::Vector16<TxidWithFee> txids)
        : MsgCombineReply<5, messages::Vector16<TCPSockaddr>, messages::Vector16<TxidWithFee>> { nonce, std::move(addresses), std::move(txids) }
    {
    }

    Error check(const PingMsg&) const;
    const messages::Vector16<TCPSockaddr>& addresses() const { return get<0>(); }
    const messages::Vector16<TxidWithFee>& txids() const { return get<1>(); }
};

struct BatchreqMsg : public MsgCombineRequest<6, BatchSelector> {
    static constexpr size_t maxSize = 14;
    std::string log_str() const;
    using MsgCombineRequest<6, BatchSelector>::MsgCombineRequest;
    // BatchreqMsg(Reader& r);
    // BatchreqMsg(BatchSelector selector)
    //     : selector(selector) {};
    // BatchreqMsg(uint32_t nonce, BatchSelector selector)
    //     : RandNonce(nonce)
    //     , selector(selector) {};
    // operator Sndbuffer() const;
    const BatchSelector& selector() const { return get<0>(); }
};

struct BatchrepMsg : public MsgCombineReply<7, messages::ReadRest<Batch>> {
    static constexpr size_t maxSize = 4 + HEADERBATCHSIZE * 80;
    using MsgCombineReply<7, messages::ReadRest<Batch>>::MsgCombineReply;
    // BatchrepMsg(Reader& r);
    // BatchrepMsg(uint32_t nonce, messages::ReadRest<Batch> b)
    //     : WithNonce { nonce }
    //     , batch(std::move(b))
    // {
    // }
    // operator Sndbuffer() const;

    const Batch& batch() const { return get<0>(); }
    Batch& batch() { return get<0>(); }
};

struct ProbereqMsg : public MsgCombineRequest<8, Descriptor, NonzeroHeightParser<EPROBEHEIGHT>> {
    static constexpr size_t maxSize = 12;
    std::string log_str() const;
    using MsgCombineRequest<8, Descriptor, NonzeroHeightParser<EPROBEHEIGHT>>::MsgCombineRequest;
    // ProbereqMsg(Reader& r);
    // ProbereqMsg(Descriptor descriptor, NonzeroHeight height)
    //     : descriptor(descriptor)
    //     , height(height)
    // {
    // }
    // operator Sndbuffer() const;
    const Descriptor& descriptor() const { return get<0>(); }
    const NonzeroHeightParser<EPROBEHEIGHT>& height() const { return get<1>(); }

    // private:
    //     ProbereqMsg(uint32_t nonce, Descriptor descriptor, NonzeroHeightParser<EPROBEHEIGHT> height)
    //         : RandNonce(nonce)
    //         , descriptor(descriptor)
    //         , height(height)
    //     {
    //     }
};

struct ProberepMsg : MsgCombineReply<9, Descriptor, CurrentAndRequested> {
    static constexpr size_t maxSize = 189;
    // ProberepMsg(Reader& r);
    using MsgCombineReply<9, Descriptor, CurrentAndRequested>::MsgCombineReply;
    ProberepMsg(uint32_t nonce, uint32_t currentDescriptor)
        : MsgCombineReply<9, Descriptor, CurrentAndRequested> { nonce, currentDescriptor, CurrentAndRequested {} }
    {
    }
    // ProberepMsg(uint32_t nonce, uint32_t currentDescriptor,
    //     CurrentAndRequested car)
    //     : WithNonce { nonce }
    //     , currentDescriptor(currentDescriptor)
    //     , currentAndRequested(std::move(car))
    // {
    // }
    auto& current() const { return currentAndRequested().current; }
    auto& current() { return currentAndRequested().current; }
    auto& requested() { return currentAndRequested().requested; }
    auto& requested() const { return currentAndRequested().requested; }
    // operator Sndbuffer() const;
    const Descriptor& currentDescriptor() const { return get<0>(); }
    const CurrentAndRequested& currentAndRequested() const { return get<1>(); }
    CurrentAndRequested& currentAndRequested() { return get<1>(); }
};

struct BlockreqMsg : public MsgCombineRequest<10, DescriptedBlockRange> {
    static constexpr size_t maxSize = 48;
    using MsgCombineRequest<10, DescriptedBlockRange>::MsgCombineRequest;

    // methods
    std::string log_str() const;
    // BlockreqMsg(DescriptedBlockRange range)
    //     : range(range) {};
    // BlockreqMsg(uint32_t nonce, DescriptedBlockRange range)
    //     : RandNonce(nonce)
    //     , range(range) {};
    // BlockreqMsg(Reader& r);
    // operator Sndbuffer() const;

    // data
    const DescriptedBlockRange& range() const { return get<0>(); }
};

struct BlockrepMsg : public MsgCombineReply<11, messages::VectorRest<BodyContainer>> {
    static constexpr size_t maxSize = MAXBLOCKBATCHSIZE * (4 + MAXBLOCKSIZE);

    using MsgCombineReply<11, messages::VectorRest<BodyContainer>>::MsgCombineReply;
    // methods
    // BlockrepMsg(Reader& r);
    // BlockrepMsg(uint32_t nonce, messages::VectorRest<BodyContainer> b)
    //     : WithNonce { nonce }
    //     , blocks(std::move(b)) {};
    // operator Sndbuffer() const;
    bool empty() const { return blocks().empty(); }
    const messages::VectorRest<BodyContainer>& blocks() const { return get<0>(); }
    messages::VectorRest<BodyContainer>& blocks() { return get<0>(); }
};

struct TxsubscribeMsg : public MsgCombineRequest<12, Height> {
    using MsgCombineRequest<12, Height>::MsgCombineRequest;
    // TxsubscribeMsg(Height upper)
    //     : upper(upper) {};
    // TxsubscribeMsg(uint32_t nonce, Height upper)
    //     : RandNonce(nonce)
    //     , upper(upper) {};
    // TxsubscribeMsg(Reader& r);
    // operator Sndbuffer() const;
    const Height& upper() const { return get<0>(); }
    static constexpr size_t maxSize = 8;
};

struct TxnotifyMsg : public MsgCombineRequest<13, messages::Vector16<TxidWithFee>> {
    static constexpr size_t MAXENTRIES = 5000;
    using MsgCombineRequest<13, messages::Vector16<TxidWithFee>>::MsgCombineRequest;
    // TxnotifyMsg(messages::Vector16<TxidWithFee> txids)
    //     : txids(std::move(txids)) {};
    // TxnotifyMsg(uint32_t nonce, messages::Vector16<TxidWithFee> txids)
    //     : RandNonce(nonce)
    //     , txids(std::move(txids)) {};
    // TxnotifyMsg(Reader& r);

    using send_iter = std::vector<mempool::Entry>::iterator;
    static Sndbuffer direct_send(send_iter begin, send_iter end);
    // operator Sndbuffer() const;
    const messages::Vector16<TxidWithFee>& txids() const { return get<0>(); }
    static constexpr size_t maxSize = 4 + TxnotifyMsg::MAXENTRIES * TransactionId::bytesize;
};

struct TxreqMsg : public MsgCombineRequest<14, messages::VectorRest<TransactionId>> {
    static constexpr size_t MAXENTRIES = 5000;
    using MsgCombineRequest<14, messages::VectorRest<TransactionId>>::MsgCombineRequest;
    // TxreqMsg(std::vector<TransactionId> txids)
    //     : txids(std::move(txids)) {};
    // TxreqMsg(uint32_t nonce, messages::VectorRest<TransactionId> txids)
    //     : RandNonce(nonce)
    //     , txids(std::move(txids)) {};
    TxreqMsg(Reader& r);
    // operator Sndbuffer() const;
    const messages::VectorRest<TransactionId>& txids() const { return get<0>(); }
    static constexpr size_t maxSize = 2 + 4 + TxreqMsg::MAXENTRIES * TransactionId::bytesize;
};

struct TxrepMsg : public MsgCombineReply<15, messages::VectorRest<messages::Optional<TransferTxExchangeMessage>>> {
    using MsgCombineReply<15, messages::VectorRest<messages::Optional<TransferTxExchangeMessage>>>::MsgCombineReply;
    using vector_t = messages::VectorRest<messages::Optional<TransferTxExchangeMessage>>;
    // TxrepMsg(vector_t txs)
    //     : txs(txs) {};
    // TxrepMsg(uint32_t nonce, messages::VectorRest<messages::Optional<TransferTxExchangeMessage>> txs)
    //     : RandNonce(nonce)
    //     , txs(txs) {};
    TxrepMsg(Reader& r);
    // operator Sndbuffer() const;
    auto& txs() const { return get<0>(); }
    static constexpr size_t maxSize = 2 + 4 + TxreqMsg::MAXENTRIES * (1 + TransferTxExchangeMessage::bytesize);
};

struct LeaderMsg : public MsgCombine<16, SignedSnapshot> {
    static constexpr size_t maxSize = 4 + 32 + 65;
    using MsgCombine<16, SignedSnapshot>::MsgCombine;
    // LeaderMsg(Reader& r);
    // LeaderMsg(SignedSnapshot snapshot)
    //     : signedSnapshot(std::move(snapshot)) {};
    // operator Sndbuffer() const;
    ///////
    // data
    const SignedSnapshot& signedSnapshot() const { return get<0>(); }
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
