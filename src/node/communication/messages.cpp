#include "messages.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "communication/messages.hpp"
#include "eventloop/types/chainstate.hpp"
#include "messages_impl.hpp"
#include "message_elements/packer_impl.hpp"


inline void throw_if_inconsistent(Height length, Worksum worksum)
{
    if ((length == 0) != worksum.is_zero()) {
        if (length == 0)
            throw Error(EFAKEWORK);
        throw Error(EFAKEHEIGHT);
    }
}




Sndbuffer InitMsg2::serialize_chainstate(const ConsensusSlave& cs)
{
    const size_t N = cs.headers().complete_batches().size();
    size_t len = 4 + 2 + 4 + 4 + 32 + 4 + N * 80;
    auto& sp { cs.get_signed_snapshot_priority() };
    auto mw { gen_msg(len) };
    mw << cs.descriptor()
       << sp.importance
       << sp.height
       << cs.headers().length()
       << cs.total_work()
       << (uint32_t)(cs.headers().complete_batches().size() * 80)
       << Range(cs.grid().raw());
    return mw;
}

InitMsg2::InitMsg2(Reader& r)
    : Base(r)
{
    if (grid().slot_end().upper() <= chainLength())
        throw Error(EGRIDMISMATCH);
    throw_if_inconsistent(chainLength(), worksum());
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

Sndbuffer InitMsg::serialize_chainstate(const ConsensusSlave& cs)
{
    const size_t N = cs.headers().complete_batches().size();
    size_t len = 4 + 2 + 4 + 4 + 32 + 4 + N * 80;
    auto& sp { cs.get_signed_snapshot_priority() };
    auto mw { gen_msg(len) };
    mw << cs.descriptor()
       << sp.importance
       << sp.height
       << cs.headers().length()
       << cs.total_work()
       << (uint32_t)(cs.headers().complete_batches().size() * 80)
       << Range(cs.grid().raw());
    return mw;
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

std::string BatchreqMsg::log_str() const
{
    return "batchreq [" + std::to_string(selector().startHeight) + "," + std::to_string(selector().startHeight + selector().length - 1) + "]";
}


std::string ProbereqMsg::log_str() const
{
    return "probereq " + std::to_string(descriptor().value()) + "/" + std::to_string(height());
}


std::string BlockreqMsg::log_str() const
{
    return "blockreq [" + std::to_string(range().lower) + "," + std::to_string(range().upper) + "]";
}

Sndbuffer TxnotifyMsg::direct_send(send_iter begin, send_iter end)
{
    auto mw { gen_msg(4 + (end - begin) * TransactionId::bytesize) };
    mw << RandNonce().nonce();
    for (auto iter = begin; iter != end; ++iter) {
        mw << iter->first;
    }
    return mw;
}

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


PingV2Msg::PingV2Msg(Reader& r)
    : PingV2Msg { r, r, r, r, r, r }
{
}

PingV2Msg::operator Sndbuffer() const
{
    return gen_msg(4 + 2 + 4 + 2 + 2 + ownRTC.byte_size() + 1)
        << nonce()
        << sp.importance
        << sp.height
        << maxAddresses
        << maxTransactions
        << ownRTC
        << maxUseOwnRTC;
}

RTCInfo::RTCInfo(Reader& r)
    : RTCInfo { r, r }
{
}

RTCInfo::operator Sndbuffer() const
{
    return gen_msg(4 + rtcPeers.byte_size())
        << nonce()
        << rtcPeers;
}

RTCSelect::RTCSelect(Reader& r)
    : RTCSelect { r, r }
{
}

RTCSelect::operator Sndbuffer() const
{
    return gen_msg(4 + selected.byte_size())
        << nonce()
        << selected;
}

RTCRequestOffer::RTCRequestOffer(Reader& r)
    : RTCRequestOffer { r, r, r }
{
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
