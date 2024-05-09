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
#include <variant>

class Reader;
class Sndbuffer;
class ConsensusSlave;
namespace mempool {
struct EntryValue;
using Entry = std::pair<TransactionId, EntryValue>;
}


struct InitMsg2 : public MsgCombine<0, Descriptor, SignedSnapshot::Priority, Height, Worksum, Grid> {
    InitMsg2(Reader& r);
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
    using Base::Base;

    auto& descriptor() const { return get<0>(); }
    const NonzeroHeight& chainLength() const { return get<1>(); }
    const Worksum& worksum() const { return get<2>(); }
    const NonzeroHeight& forkHeight() const { return get<3>(); }
    const messages::ReadRest<Grid>& grid() const { return get<4>(); }
};
struct AppendMsg : public MsgCombine<2, NonzeroHeight, Worksum, messages::ReadRest<Grid>> {
    using Base::Base;
    static constexpr size_t maxSize = 4 + 4 + 32 + 80 * 100;

    auto& newLength() const { return get<0>(); };
    auto& worksum() const { return get<1>(); };
    auto& grid() const { return get<2>(); };
};

struct SignedPinRollbackMsg : public MsgCombine<3, SignedSnapshot, Height, Worksum, Descriptor> {
    static constexpr size_t maxSize = SignedSnapshot::binary_size + 4 + 32 + 4;
    using Base::Base;

    const SignedSnapshot& signedSnapshot() const { return get<0>(); }
    const Height& shrinkLength() const { return get<1>(); }
    const Worksum& worksum() const { return get<2>(); }
    const Descriptor& descriptor() const { return get<3>(); }
};

struct PingMsg : public MsgCombineRequest<4, SignedSnapshot::Priority, uint16_t, uint16_t> {
    static constexpr size_t maxSize = 30; // actually 14 but be generous to avoid bugs;
    using Base::Base;

    PingMsg(SignedSnapshot::Priority sp, uint16_t maxAddresses = 5, uint16_t maxTransactions = 100)
        : Base(sp, maxAddresses, maxTransactions)
    {
    }
    const SignedSnapshot::Priority& sp() const { return get<0>(); }
    const uint16_t& maxAddresses() const { return get<1>(); }
    const uint16_t& maxTransactions() const { return get<2>(); }
};

struct PongMsg : public MsgCombineReply<5, messages::Vector16<TCPSockaddr>, messages::Vector16<TxidWithFee>> {
    static constexpr size_t maxSize = 4 + 2 + 6 * 100 + 18 * 1000;
    using Base::Base;
    PongMsg(uint32_t nonce, messages::Vector16<TCPSockaddr> addresses, messages::Vector16<TxidWithFee> txids)
        : Base { nonce, std::move(addresses), std::move(txids) }
    {
    }

    Error check(const PingMsg&) const;
    const messages::Vector16<TCPSockaddr>& addresses() const { return get<0>(); }
    const messages::Vector16<TxidWithFee>& txids() const { return get<1>(); }
};

struct BatchreqMsg : public MsgCombineRequest<6, BatchSelector> {
    static constexpr size_t maxSize = 14;
    using Base::Base;

    std::string log_str() const;
    const BatchSelector& selector() const { return get<0>(); }
};

struct BatchrepMsg : public MsgCombineReply<7, messages::ReadRest<Batch>> {
    static constexpr size_t maxSize = 4 + HEADERBATCHSIZE * 80;
    using Base::Base;

    const Batch& batch() const { return get<0>(); }
    Batch& batch() { return get<0>(); }
};

struct ProbereqMsg : public MsgCombineRequest<8, Descriptor, NonzeroHeightParser<EPROBEHEIGHT>> {
    static constexpr size_t maxSize = 12;
    using Base::Base;

    std::string log_str() const;
    const Descriptor& descriptor() const { return get<0>(); }
    const NonzeroHeightParser<EPROBEHEIGHT>& height() const { return get<1>(); }
};

struct ProberepMsg : MsgCombineReply<9, Descriptor, CurrentAndRequested> {
    static constexpr size_t maxSize = 189;
    using Base::Base;

    ProberepMsg(uint32_t nonce, uint32_t currentDescriptor)
        : MsgCombineReply<9, Descriptor, CurrentAndRequested> { nonce, currentDescriptor, CurrentAndRequested {} }
    {
    }
    auto& current() const { return currentAndRequested().current; }
    auto& current() { return currentAndRequested().current; }
    auto& requested() { return currentAndRequested().requested; }
    auto& requested() const { return currentAndRequested().requested; }
    const Descriptor& currentDescriptor() const { return get<0>(); }
    const CurrentAndRequested& currentAndRequested() const { return get<1>(); }
    CurrentAndRequested& currentAndRequested() { return get<1>(); }
};

struct BlockreqMsg : public MsgCombineRequest<10, DescriptedBlockRange> {
    static constexpr size_t maxSize = 48;
    using Base::Base;

    std::string log_str() const;
    const DescriptedBlockRange& range() const { return get<0>(); }
};

struct BlockrepMsg : public MsgCombineReply<11, messages::VectorRest<BodyContainer>> {
    static constexpr size_t maxSize = MAXBLOCKBATCHSIZE * (4 + MAXBLOCKSIZE);
    using Base::Base;

    bool empty() const { return blocks().empty(); }
    const messages::VectorRest<BodyContainer>& blocks() const { return get<0>(); }
    messages::VectorRest<BodyContainer>& blocks() { return get<0>(); }
};

struct TxsubscribeMsg : public MsgCombineRequest<12, Height> {
    static constexpr size_t maxSize = 8;
    using Base::Base;

    const Height& upper() const { return get<0>(); }
};

struct TxnotifyMsg : public MsgCombineRequest<13, messages::Vector16<TxidWithFee>> {
    static constexpr size_t MAXENTRIES = 5000;
    using Base::Base;

    using send_iter = std::vector<mempool::Entry>::iterator;
    static Sndbuffer direct_send(send_iter begin, send_iter end);
    const messages::Vector16<TxidWithFee>& txids() const { return get<0>(); }
    static constexpr size_t maxSize = 4 + TxnotifyMsg::MAXENTRIES * TransactionId::bytesize;
};

struct TxreqMsg : public MsgCombineRequest<14, messages::VectorRest<TransactionId>> {
    static constexpr size_t MAXENTRIES = 5000;
    using Base::Base;

    TxreqMsg(Reader& r);
    const messages::VectorRest<TransactionId>& txids() const { return get<0>(); }
    static constexpr size_t maxSize = 2 + 4 + TxreqMsg::MAXENTRIES * TransactionId::bytesize;
};

struct TxrepMsg : public MsgCombineReply<15, messages::VectorRest<messages::Optional<TransferTxExchangeMessage>>> {
    static constexpr size_t maxSize = 2 + 4 + TxreqMsg::MAXENTRIES * (1 + TransferTxExchangeMessage::bytesize);
    using Base::Base;

    using vector_t = messages::VectorRest<messages::Optional<TransferTxExchangeMessage>>;
    TxrepMsg(Reader& r);
    auto& txs() const { return get<0>(); }
};

struct LeaderMsg : public MsgCombine<16, SignedSnapshot> {
    static constexpr size_t maxSize = 4 + 32 + 65;
    using Base::Base;

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
