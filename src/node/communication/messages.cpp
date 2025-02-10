#include "messages.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "communication/messages.hpp"
#include "eventloop/types/chainstate.hpp"
#include "message_elements/packer_impl.hpp"
#include "messages_impl.hpp"
#include "spdlog/fmt/fmt.h"

using namespace std::string_literals;
namespace fmt_lib = spdlog::fmt_lib;
inline void throw_if_inconsistent(Height length, Worksum worksum)
{
    if ((length == 0) != worksum.is_zero()) {
        if (length == 0)
            throw Error(EFAKEWORK);
        throw Error(EFAKEHEIGHT);
    }
}

InitMsgV1::InitMsgV1(Reader& r)
    : descriptor(r)
    , sp(r)
    , chainLength(r)
    , worksum(r)
    , grid(r)
{
    if (grid.slot_end().upper() <= chainLength)
        throw Error(EGRIDMISMATCH);
    throw_if_inconsistent(chainLength, worksum);
}

std::string InitMsgV1::log_str() const
{
    return fmt_lib::format("InitMsgV1, descriptor {}, length {}, worksum {}, grid size {}", descriptor.value(), chainLength.value(), worksum.to_string(), grid.size());
}
InitMsgGeneratorV1::operator Sndbuffer() const
{
    const size_t N = cs.headers().complete_batches().size();
    size_t len = 4 + 2 + 4 + 4 + 32 + 4 + N * 80;
    auto& sp { cs.get_signed_snapshot_priority() };
    auto mw { MsgCode<0>::gen_msg(len) };
    mw << cs.descriptor()
       << sp.importance
       << sp.height
       << cs.headers().length()
       << cs.total_work()
       << (uint32_t)(cs.headers().complete_batches().size() * 80)
       << Range(cs.grid().raw());
    return mw;
}

std::string InitMsgGeneratorV1::log_str() const
{
    return fmt_lib::format("InitMsgV1, descriptor {}, length {}, worksum {}, grid size {}", cs.descriptor().value(), cs.headers().length().value(), cs.total_work().to_string(), cs.grid().size());
}

std::string ForkMsg::log_str() const
{
    return fmt_lib::format("ForkMsg, descriptor {}, length {}, worksum {}, forkHeight {}, grid size {}", descriptor().value(), chainLength().value(), worksum().to_string(), forkHeight().value(), grid().size());
}

std::string AppendMsg::log_str() const
{
    return fmt_lib::format("AppendMsg, length {}, worksum {}, grid size {}", newLength().value(), worksum().to_string(), grid().size());
}

std::string SignedPinRollbackMsg::log_str() const
{
    return fmt_lib::format("SignedPinRollbackMsg, shrinkLength {}, worksum {}, descriptor {}", shrinkLength().value(), worksum().to_string(), descriptor().value());
}
std::string PingMsg::log_str() const
{
    return fmt_lib::format("PingMsg, maxAddresses {}, maxTransactions {}", maxAddresses(), maxTransactions());
}
std::string PongMsg::log_str() const
{
    return fmt_lib::format("PongMsg, addresses {}, txids {}", addresses().size(), txids().size());
}
std::string BatchreqMsg::log_str() const
{
    return fmt_lib::format("BatchreqMsg [{},{}]", std::to_string(selector().startHeight), std::to_string(selector().startHeight + selector().length - 1));
}

std::string BatchrepMsg::log_str() const
{
    return fmt_lib::format("BatchrepMsg, size: {}", batch().size());
}

std::string ProbereqMsg::log_str() const
{
    return fmt_lib::format("ProbereqMsg {}/{}", std::to_string(descriptor().value()), std::to_string(height()));
}

std::string ProberepMsg::log_str() const
{
    ;
    return fmt_lib::format("ProberepMsg {}/{}", current().has_value(), requested().has_value());
}

std::string BlockreqMsg::log_str() const
{
    return fmt_lib::format("BlockreqMsg {}/[{},{}]", range().descriptor.value(), range().first().value(), range().last().value());
}
std::string BlockrepMsg::log_str() const
{
    return fmt_lib::format("BlockrepMsg size {}", blocks().size());
}

std::string TxnotifyMsg::log_str() const
{
    return fmt_lib::format("TxnotifyMsg size {}", txids().size());
}

std::string TxreqMsg::log_str() const
{
    return fmt_lib::format("TxreqMsg size {}", txids().size());
}

std::string TxrepMsg::log_str() const
{
    return fmt_lib::format("TxrepMsg size {}", txs().size());
}

std::string LeaderMsg::log_str() const
{
    return fmt_lib::format("LeaderMsg priority {}", static_cast<std::string>(signedSnapshot().priority));
}

std::string RTCIdentity::log_str() const
{
    auto ip_string = [](auto& ip) -> std::string {
        if (ip)
            return ip->to_string();
        return "none"s;
    };
    return fmt_lib::format("RTCIdentity IPv4: {}, IPv6: {}", ip_string(ips().get_ip4()), ip_string(ips().get_ip6()));
}

std::string RTCQuota::log_str() const
{
    return fmt_lib::format("RTCQuota +{}", increase());
}

std::string RTCSignalingList::log_str() const
{
    return fmt_lib::format("RTCSignalingList {} ips", ips().size());
}

std::string RTCRequestForwardOffer::log_str() const
{
    return fmt_lib::format("RTCRequestForwardOffer key {}, size: {}", signaling_list_key(), offer().size());
}

std::string RTCForwardedOffer::log_str() const
{
    return fmt_lib::format("RTCForwardedOffer size {}", offer().size());
}
std::string RTCRequestForwardAnswer::log_str() const
{
    return fmt_lib::format("RTCRequestForwardAnswer key {}, size: {}", key(), answer().data.size());
}

std::string RTCForwardOfferDenied::log_str() const
{
    return fmt_lib::format("RTCForwardOfferDenied key {}, reason: {}", key(), reason());
}
std::string RTCForwardedAnswer::log_str() const
{
    return fmt_lib::format("RTCForwardedAnswer key {}, size: {}", key(), answer().size());
}
std::string RTCVerificationOffer::log_str() const
{
    return fmt_lib::format("RTCVerificationOffer ip {}, offer.size()", ip().to_string(), offer().size());
}
std::string RTCVerificationAnswer::log_str() const
{
    return fmt_lib::format("RTCVerificationAnswer size()", answer().size());
}
std::string PingV2Msg::log_str() const
{
    return fmt_lib::format("PingV2Msg ip maxAddresses {}, maxTransactions {}, discarded_forward_requests {}", maxAddresses(), maxTransactions(), discarded_forward_requests());
}

std::string PongV2Msg::log_str() const
{
    return fmt_lib::format("PongV2Msg ip addresses {}, txids {}", addresses().size(), txids().size());
}

Error PongMsg::check(const PingMsg& m) const
{
    if (nonce() != m.nonce())
        return EUNREQUESTED;
    if (addresses().size() > m.maxAddresses()
        || txids().size() > m.maxTransactions()) {
        return ERESTRICTED;
    }
    return 0;
}

// Sndbuffer TxnotifyMsg::direct_send(send_iter begin, send_iter end)
// {
//     TODO: wrong since it is not including 16 bit length of #transactions
//     auto mw { gen_msg(4 + (end - begin) * TransactionId::bytesize) };
//     mw << RandNonce().nonce();
//     for (auto iter = begin; iter != end; ++iter) {
//         mw << iter->transaction_id();
//     }
//     return mw;
// }

TxreqMsg::TxreqMsg(Reader& r)
    : Base(r)
{
    if (txids().size() > MAXENTRIES)
        throw Error(EINV_TXREQ);
}

TxrepMsg::TxrepMsg(Reader& r)
    : Base(r)
{
    if (txs().size() > TxreqMsg::MAXENTRIES)
        throw Error(EINV_TXREP);
}

Error PongV2Msg::check(const PingV2Msg& m) const
{
    if (nonce() != m.nonce())
        return EUNREQUESTED;
    if (addresses().size() > m.maxAddresses()
        || txids().size() > m.maxTransactions()) {
        return ERESTRICTED;
    }
    return 0;
}

InitMsgV3::InitMsgV3(Reader& r)
    : Base(r)
{
    if (grid().slot_end().upper() <= chain_length())
        throw Error(EGRIDMISMATCH);
    throw_if_inconsistent(chain_length(), worksum());
}

std::string InitMsgV3::log_str() const
{
    return fmt_lib::format("InitMsg3, descriptor {}, length {}, worksum {}, grid size {}", descriptor().value(), chain_length().value(), worksum().to_string(), grid().size());
}

InitMsgGeneratorV3::operator Sndbuffer() const
{
    const size_t N = cs.headers().complete_batches().size();
    size_t len = 4 + 2 + 4 + 4 + 32 + 4 + N * 80 + 1;
    auto& sp { cs.get_signed_snapshot_priority() };
    auto mw { MsgCode<0>::gen_msg(len) };
    mw << cs.descriptor()
       << sp.importance
       << sp.height
       << cs.headers().length()
       << cs.total_work()
       << (uint32_t)(cs.headers().complete_batches().size() * 80)
       << Range(cs.grid().raw())
       << uint8_t(rtcEnabled);
    return mw;
}

std::string InitMsgGeneratorV3::log_str() const
{
    return fmt_lib::format("InitMsgV3, descriptor {}, length {}, worksum {}, grid size {}", cs.descriptor().value(), cs.headers().length().value(), cs.total_work().to_string(), cs.grid().size());
}

namespace {
template <uint8_t prevcode>
size_t size_bound(uint8_t)
{
    return 0;
}
template <uint8_t prevcode, typename T, typename... S>
size_t size_bound(uint8_t type)
{
    // variant types must be in order and message codes must be all different
    static_assert(prevcode < T::msgcode);
    if (T::msgcode == type)
        return T::maxSize;
    return size_bound<T::msgcode, S...>(type);
}

template <typename T, typename... S>
size_t size_bound(uint8_t type)
{
    if (T::msgcode == type)
        return T::maxSize;
    return size_bound<T::msgcode, S...>(type);
}

// do metaprogramming dance
template <typename T>
class TypeExtractor {
};

template <typename... Types>
class TypeExtractor<std::variant<Types...>> {
public:
    static auto size_bound(uint8_t type)
    {
        return ::size_bound<Types...>(type);
    }
};

}

namespace messages {
size_t size_bound(uint8_t msgtype)
{
    return TypeExtractor<messages::Msg>::size_bound(msgtype);
}
}
