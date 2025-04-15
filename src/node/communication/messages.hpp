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
#include "transport/helpers/ip.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include "transport/webrtc/sdp_util.hpp"
#include <variant>

class Reader;
class Sndbuffer;
class ConsensusSlave;
namespace mempool {
struct EntryValue;
class Entry;
}

namespace spdlog {
class logger;
}

struct InitMsgGeneratorV1 {
    const ConsensusSlave& cs;
    operator Sndbuffer() const;
    std::string log_str() const;
};
struct InitMsgGeneratorV3 {
    const ConsensusSlave& cs;
    bool rtcEnabled;
    InitMsgGeneratorV3(const ConsensusSlave& cs, bool rtcEnabled)
        : cs(cs)
        , rtcEnabled(rtcEnabled)
    {
    }
    operator Sndbuffer() const;
    std::string log_str() const;
};

struct InitMsgV1 : public MsgCode<0> {
    InitMsgV1(Reader& r);
    static constexpr size_t maxSize = 100000;
    std::string log_str() const;

    Descriptor descriptor;
    SignedSnapshot::Priority sp;
    Height chainLength;
    Worksum worksum;
    Grid grid;
};

struct ForkMsg : public MsgCombine<1, Descriptor, NonzeroHeight, Worksum, NonzeroHeight, messages::ReadRest<Grid>> {
    static constexpr size_t maxSize = 20000;
    using Base::Base;
    std::string log_str() const;

    [[nodiscard]] auto& descriptor() const { return get<0>(); }
    [[nodiscard]] const NonzeroHeight& chainLength() const { return get<1>(); }
    [[nodiscard]] const Worksum& worksum() const { return get<2>(); }
    [[nodiscard]] const NonzeroHeight& forkHeight() const { return get<3>(); }
    [[nodiscard]] const messages::ReadRest<Grid>& grid() const { return get<4>(); }
};
struct AppendMsg : public MsgCombine<2, NonzeroHeight, Worksum, messages::ReadRest<Grid>> {
    using Base::Base;
    static constexpr size_t maxSize = 4 + 4 + 32 + 80 * 100;

    std::string log_str() const;
    [[nodiscard]] auto& newLength() const { return get<0>(); };
    [[nodiscard]] auto& worksum() const { return get<1>(); };
    [[nodiscard]] auto& grid() const { return get<2>(); };
};

struct SignedPinRollbackMsg : public MsgCombine<3, SignedSnapshot, Height, Worksum, Descriptor> {
    static constexpr size_t maxSize = SignedSnapshot::binary_size + 4 + 32 + 4;
    using Base::Base;

    std::string log_str() const;
    [[nodiscard]] const SignedSnapshot& signedSnapshot() const { return get<0>(); }
    [[nodiscard]] const Height& shrinkLength() const { return get<1>(); }
    [[nodiscard]] const Worksum& worksum() const { return get<2>(); }
    [[nodiscard]] const Descriptor& descriptor() const { return get<3>(); }
};

struct PingMsg : public MsgCombineRequest<4, SignedSnapshot::Priority, uint16_t, uint16_t> {
    static constexpr size_t maxSize = 30; // actually 14 but be generous to avoid bugs;
    using Base::Base;

    PingMsg(SignedSnapshot::Priority sp, uint16_t maxAddresses = 5, uint16_t maxTransactions = 100)
        : Base(sp, maxAddresses, maxTransactions)
    {
    }
    std::string log_str() const;
    [[nodiscard]] const SignedSnapshot::Priority& sp() const { return get<0>(); }
    [[nodiscard]] const uint16_t& maxAddresses() const { return get<1>(); }
    [[nodiscard]] const uint16_t& maxTransactions() const { return get<2>(); }
};

struct PongMsg : public MsgCombineReply<5, messages::Vector16<TCPPeeraddr>, messages::Vector16<TxidWithFee>> {
    static constexpr size_t maxSize = 4 + 2 + 6 * 100 + 18 * 1000;
    using Base::Base;
    std::string log_str() const;
    PongMsg(uint32_t nonce, messages::Vector16<TCPPeeraddr> addresses, messages::Vector16<TxidWithFee> txids)
        : Base { nonce, std::move(addresses), std::move(txids) }
    {
    }

    Error check(const PingMsg&) const;
    [[nodiscard]] const messages::Vector16<TCPPeeraddr>& addresses() const { return get<0>(); }
    [[nodiscard]] const messages::Vector16<TxidWithFee>& txids() const { return get<1>(); }
};

struct BatchreqMsg : public MsgCombineRequest<6, BatchSelector> {
    static constexpr size_t maxSize = 14;
    using Base::Base;

    std::string log_str() const;
    [[nodiscard]] const BatchSelector& selector() const { return get<0>(); }
};

struct BatchrepMsg : public MsgCombineReply<7, messages::ReadRest<Batch>> {
    static constexpr size_t maxSize = 4 + HEADERBATCHSIZE * 80;
    using Base::Base;

    std::string log_str() const;
    [[nodiscard]] const Batch& batch() const { return get<0>(); }
    Batch& batch() { return get<0>(); }
};

struct ProbereqMsg : public MsgCombineRequest<8, Descriptor, NonzeroHeightParser<EPROBEHEIGHT>> {
    static constexpr size_t maxSize = 12;
    using Base::Base;

    std::string log_str() const;
    [[nodiscard]] const Descriptor& descriptor() const { return get<0>(); }
    [[nodiscard]] const NonzeroHeightParser<EPROBEHEIGHT>& height() const { return get<1>(); }
};

struct ProberepMsg : MsgCombineReply<9, Descriptor, CurrentAndRequested> {
    static constexpr size_t maxSize = 189;
    using Base::Base;

    std::string log_str() const;
    ProberepMsg(uint32_t nonce, uint32_t currentDescriptor)
        : MsgCombineReply<9, Descriptor, CurrentAndRequested> { nonce, currentDescriptor, CurrentAndRequested {} }
    {
    }
    [[nodiscard]] auto& current() const { return currentAndRequested().current; }
    [[nodiscard]] auto& current() { return currentAndRequested().current; }
    [[nodiscard]] auto& requested() { return currentAndRequested().requested; }
    [[nodiscard]] auto& requested() const { return currentAndRequested().requested; }
    [[nodiscard]] const Descriptor& currentDescriptor() const { return get<0>(); }
    [[nodiscard]] const CurrentAndRequested& currentAndRequested() const { return get<1>(); }
    CurrentAndRequested& currentAndRequested() { return get<1>(); }
};

struct BlockreqMsg : public MsgCombineRequest<10, DescriptedBlockRange> {
    static constexpr size_t maxSize = 48;
    using Base::Base;

    std::string log_str() const;
    [[nodiscard]] const DescriptedBlockRange& range() const { return get<0>(); }
};

struct BlockrepMsg : public MsgCombineReply<11, messages::VectorRest<BodyContainer>> {
    static constexpr size_t maxSize = MAXBLOCKBATCHSIZE * (4 + MAXBLOCKSIZE);
    using Base::Base;

    std::string log_str() const;
    bool empty() const { return blocks().empty(); }
    [[nodiscard]] const messages::VectorRest<BodyContainer>& blocks() const { return get<0>(); }
    messages::VectorRest<BodyContainer>& block_bodies() { return get<0>(); }
};

struct TxsubscribeMsg : public MsgCombineRequest<12, Height> {
    static constexpr size_t maxSize = 8;
    using Base::Base;

    std::string log_str() const;
    [[nodiscard]] const Height& upper() const { return get<0>(); }
};

struct TxnotifyMsg : public MsgCombineRequest<13, messages::Vector16<TxidWithFee>> {
    static constexpr size_t MAXENTRIES = 5000;
    using Base::Base;

    using send_iter = std::vector<mempool::Entry>::iterator;
    // static Sndbuffer direct_send(send_iter begin, send_iter end);
    [[nodiscard]] const messages::Vector16<TxidWithFee>& txids() const { return get<0>(); }
    static constexpr size_t maxSize = 4 + TxnotifyMsg::MAXENTRIES * TransactionId::bytesize;
    std::string log_str() const;
};

struct TxreqMsg : public MsgCombineRequest<14, messages::VectorRest<TransactionId>> {
    static constexpr size_t MAXENTRIES = 5000;
    using Base::Base;

    TxreqMsg(Reader& r);
    [[nodiscard]] const messages::VectorRest<TransactionId>& txids() const { return get<0>(); }
    static constexpr size_t maxSize = 2 + 4 + TxreqMsg::MAXENTRIES * TransactionId::bytesize;
    std::string log_str() const;
};

struct TxrepMsg : public MsgCombineReply<15, messages::VectorRest<messages::Optional<WartTransferMessage>>> {
    static constexpr size_t maxSize = 2 + 4 + TxreqMsg::MAXENTRIES * (1 + WartTransferMessage::bytesize);
    using Base::Base;

    using vector_t = messages::VectorRest<messages::Optional<WartTransferMessage>>;
    TxrepMsg(Reader& r);
    [[nodiscard]] auto& txs() const { return get<0>(); }
    std::string log_str() const;
};

struct LeaderMsg : public MsgCombine<16, SignedSnapshot> {
    static constexpr size_t maxSize = 4 + 32 + 65;
    using Base::Base;

    [[nodiscard]] const SignedSnapshot& signedSnapshot() const { return get<0>(); }
    std::string log_str() const;
};

struct RTCIdentity : public MsgCombineRequest<18, IdentityIps> {
    static constexpr size_t maxSize = 1000;
    using Base::Base;

    [[nodiscard]] auto& ips() const { return get<0>(); }
    std::string log_str() const;
};

struct RTCQuota : public MsgCombine<19, uint8_t> {
    static constexpr size_t maxSize = 100000;
    using Base::Base;

    [[nodiscard]] uint8_t increase() const { return get<0>(); }
    std::string log_str() const;
};

struct RTCSignalingList : public MsgCombine<20, messages::Vector8<IP>> {
    static constexpr size_t maxSize = 100000;
    using Base::Base;

    [[nodiscard]] const auto& ips() const { return get<0>(); }
    std::string log_str() const;
};

struct RTCRequestForwardOffer : public MsgCombine<21, uint64_t, String16> {
    static constexpr size_t maxSize = 10;
    using Base::Base;

    [[nodiscard]] const auto& signaling_list_key() const { return get<0>(); }
    [[nodiscard]] const std::string& offer() const { return get<1>().data; }
    std::string log_str() const;
};

struct RTCForwardedOffer : public MsgCombine<22, String16> {
    static constexpr size_t maxSize = 100000;
    using Base::Base;

    [[nodiscard]] const std::string& offer() const { return get<0>().data; }
    std::string log_str() const;
};

struct RTCRequestForwardAnswer : public MsgCombine<23, String16, uint64_t> {
    static constexpr size_t maxSize = 100000;
    using Base::Base;

    [[nodiscard]] const String16& answer() const { return get<0>(); }
    [[nodiscard]] String16& answer() { return get<0>(); }
    [[nodiscard]] auto& key() const { return get<1>(); }
    std::string log_str() const;
};

struct RTCForwardOfferDenied : public MsgCombine<24, uint32_t, uint8_t> {
    static constexpr size_t maxSize = 100000;
    using Base::Base;

    [[nodiscard]] auto key() const { return get<0>(); }
    [[nodiscard]] auto reason() const { return get<1>(); }
    std::string log_str() const;
};

struct RTCForwardedAnswer : public MsgCombine<25, uint32_t, String16> {
    static constexpr size_t maxSize = 100000;
    using Base::Base;

    [[nodiscard]] uint32_t key() const { return get<0>(); }
    [[nodiscard]] const std::string& answer() const { return get<1>().data; }
    std::string log_str() const;
};

struct RTCVerificationOffer : public MsgCombine<26, IP, String16> {
    static constexpr size_t maxSize = 100000;
    using Base::Base;

    [[nodiscard]] const auto& ip() const { return get<0>(); }
    [[nodiscard]] const std::string& offer() const { return get<1>().data; }
    std::string log_str() const;
};

struct RTCVerificationAnswer : public MsgCombine<27, String16> {
    static constexpr size_t maxSize = 100000;
    using Base::Base;

    [[nodiscard]] const std::string& answer() const { return get<0>().data; }
    std::string log_str() const;
};

struct PingV2Msg : public MsgCombineRequest<28, SignedSnapshot::Priority, uint16_t, uint16_t, uint32_t> {
    static constexpr size_t maxSize = 40; //
    using Base::Base;
    struct Options {
        uint16_t maxAddresses = 5;
        uint16_t maxTransactions = 100;
        uint32_t ndiscard = 0;
        std::string log_str() const;
    };

    PingV2Msg(SignedSnapshot::Priority sp, Options o)
        : Base(sp, o.maxAddresses, o.maxTransactions, o.ndiscard)
    {
    }
    [[nodiscard]] const SignedSnapshot::Priority& sp() const { return get<0>(); }
    [[nodiscard]] const uint16_t& maxAddresses() const { return get<1>(); }
    [[nodiscard]] const uint16_t& maxTransactions() const { return get<2>(); }
    [[nodiscard]] const uint32_t& discarded_forward_requests() const { return get<3>(); }
    std::string log_str() const;
    // [[nodiscard]] const uint32_t& minForwardKey() const { return get<3>(); }
};

struct PongV2Msg : public MsgCombineReply<29, messages::Vector16<TCPPeeraddr>, messages::Vector16<TxidWithFee>> {
    static constexpr size_t maxSize = 4 + 2 + 6 * 100 + 18 * 1000;
    using Base::Base;
    PongV2Msg(uint32_t nonce, messages::Vector16<TCPPeeraddr> addresses, messages::Vector16<TxidWithFee> txids)
        : Base { nonce, std::move(addresses), std::move(txids) }
    {
    }

    Error check(const PingV2Msg&) const;
    [[nodiscard]] const messages::Vector16<TCPPeeraddr>& addresses() const { return get<0>(); }
    [[nodiscard]] const messages::Vector16<TxidWithFee>& txids() const { return get<1>(); }
    std::string log_str() const;
};

struct InitMsgV3 : public MsgCombine<30, Descriptor, SignedSnapshot::Priority, Height, Worksum, Grid, uint8_t> {
    InitMsgV3(Reader& r);
    static constexpr size_t maxSize = 100000;
    std::string log_str() const;

    [[nodiscard]] auto& descriptor() const { return get<0>(); }
    [[nodiscard]] auto& sp() const { return get<1>(); }
    [[nodiscard]] auto& chain_length() const { return get<2>(); }
    [[nodiscard]] auto& worksum() const { return get<3>(); }
    [[nodiscard]] auto& grid() const { return get<4>(); }
    [[nodiscard]] auto rtc_enabled() const { return (get<5>() & 1) != 0; }
};

namespace messages {
[[nodiscard]] size_t size_bound(uint8_t msgtype);

using Msg = std::variant<InitMsgV1, ForkMsg, AppendMsg, SignedPinRollbackMsg, PingMsg, PongMsg, BatchreqMsg, BatchrepMsg, ProbereqMsg, ProberepMsg, BlockreqMsg, BlockrepMsg, TxnotifyMsg, TxreqMsg, TxrepMsg, LeaderMsg, RTCIdentity, RTCQuota, RTCSignalingList, RTCRequestForwardOffer, RTCForwardedOffer, RTCRequestForwardAnswer, RTCForwardOfferDenied, RTCForwardedAnswer, RTCVerificationOffer, RTCVerificationAnswer, PingV2Msg, PongV2Msg, InitMsgV3>;
} // namespace messages
