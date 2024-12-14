#include "messages.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "communication/messages.hpp"
#include "eventloop/types/chainstate.hpp"
#include "message_elements/packer_impl.hpp"
#include "messages_impl.hpp"
#include "spdlog/fmt/bundled/core.h"

using namespace std::string_literals;
inline void throw_if_inconsistent(Height length, Worksum worksum)
{
    if ((length == 0) != worksum.is_zero()) {
        if (length == 0)
            throw Error(EFAKEWORK);
        throw Error(EFAKEHEIGHT);
    }
}

InitMsg2::InitMsg2(Reader& r)
    : Base(r)
{
    if (grid().slot_end().upper() <= chain_length())
        throw Error(EGRIDMISMATCH);
    throw_if_inconsistent(chain_length(), worksum());
}

std::string InitMsg2::log_str() const
{
    return fmt::format("InitMsg2, descriptor {}, length {}, worksum {}, grid size {}", descriptor().value(), chain_length().value(), worksum().to_string(), grid().size());
}

InitMsg::InitMsg(Reader& r)
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

InitMsgGenerator::operator Sndbuffer() const
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

std::string InitMsgGenerator::log_str() const
{
    return fmt::format("InitMsg, descriptor {}, length {}, worksum {}, grid size {}", cs.descriptor().value(), cs.headers().length().value(), cs.total_work().to_string(), cs.grid().size());
}
std::string InitMsg::log_str() const
{
    return fmt::format("InitMsg, descriptor {}, length {}, worksum {}, grid size {}", descriptor.value(), chainLength.value(), worksum.to_string(), grid.size());
}

std::string ForkMsg::log_str() const
{
    return fmt::format("ForkMsg, descriptor {}, length {}, worksum {}, forkHeight {}, grid size {}", descriptor().value(), chainLength().value(), worksum().to_string(), forkHeight().value(), grid().size());
}

std::string AppendMsg::log_str() const
{
    return fmt::format("AppendMsg, length {}, worksum {}, grid size {}", newLength().value(), worksum().to_string(), grid().size());
}

std::string SignedPinRollbackMsg::log_str() const
{
    return fmt::format("SignedPinRollbackMsg, shrinkLength {}, worksum {}, descriptor {}", shrinkLength().value(), worksum().to_string(), descriptor().value());
}
std::string PingMsg::log_str() const
{
    return fmt::format("PingMsg, maxAddresses {}, maxTransactions {}", maxAddresses(), maxTransactions());
}
std::string PongMsg::log_str() const
{
    return fmt::format("PongMsg, addresses {}, txids {}", addresses().size(), txids().size());
}
std::string BatchreqMsg::log_str() const
{
    return fmt::format("BatchreqMsg [{},{}]", std::to_string(selector().startHeight), std::to_string(selector().startHeight + selector().length - 1));
}

std::string BatchrepMsg::log_str() const
{
    return fmt::format("BatchrepMsg, size: {}", batch().size());
}

std::string ProbereqMsg::log_str() const
{
    return fmt::format("ProbereqMsg {}/{}", std::to_string(descriptor().value()), std::to_string(height()));
}

std::string ProberepMsg::log_str() const
{
    ;
    return fmt::format("ProberepMsg {}/{}", current().has_value(), requested().has_value());
}

std::string BlockreqMsg::log_str() const
{
    return fmt::format("BlockreqMsg {}/[{},{}]", range().descriptor.value(), range().lower.value(), range().upper.value());
}
std::string BlockrepMsg::log_str() const
{
    return fmt::format("BlockrepMsg size {}", blocks().size());
}

std::string TxnotifyMsg::log_str() const
{
    return fmt::format("TxnotifyMsg size {}", txids().size());
}

std::string TxreqMsg::log_str() const
{
    return fmt::format("TxreqMsg size {}", txids().size());
}

std::string TxrepMsg::log_str() const
{
    return fmt::format("TxrepMsg size {}", txs().size());
}

std::string LeaderMsg::log_str() const
{
    return fmt::format("LeaderMsg priority {}", static_cast<std::string>(signedSnapshot().priority));
}

std::string RTCIdentity::log_str() const
{
    auto ip_string = [](auto& ip) -> std::string {
        if (ip)
            return ip->to_string();
        return "none"s;
    };
    return fmt::format("RTCIdentity IPv4: {}, IPv6: {}", ip_string(ips().get_ip4()), ip_string(ips().get_ip6()));
}

std::string RTCQuota::log_str() const
{
    return fmt::format("RTCQuota +{}", increase());
}

std::string RTCSignalingList::log_str() const
{
    return fmt::format("RTCSignalingList {} ips", ips().size());
}

std::string RTCRequestForwardOffer::log_str() const
{
    return fmt::format("RTCRequestForwardOffer key {}, size: {}", signaling_list_key(), offer().size());
}

std::string RTCForwardedOffer::log_str() const
{
    return fmt::format("RTCForwardedOffer size {}", offer().size());
}
std::string RTCRequestForwardAnswer::log_str() const
{
    return fmt::format("RTCRequestForwardAnswer key {}, size: {}", key(), answer().data.size());
}

std::string RTCForwardOfferDenied::log_str() const
{
    return fmt::format("RTCForwardOfferDenied key {}, reason: {}", key(), reason());
}
std::string RTCForwardedAnswer::log_str() const
{
    return fmt::format("RTCForwardedAnswer key {}, size: {}", key(), answer().size());
}
std::string RTCVerificationOffer::log_str() const
{
    return fmt::format("RTCVerificationOffer ip {}, offer.size()", ip().to_string(), offer().size());
}
std::string RTCVerificationAnswer::log_str() const
{
    return fmt::format("RTCVerificationAnswer size()", answer().size());
}
std::string PingV2Msg::log_str() const
{
    return fmt::format("PingV2Msg ip maxAddresses {}, maxTransactions {}, discarded_forward_requests {}", maxAddresses(), maxTransactions(), discarded_forward_requests());
}

std::string PongV2Msg::log_str() const
{
    return fmt::format("PongV2Msg ip addresses {}, txids {}", addresses().size(), txids().size());
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
