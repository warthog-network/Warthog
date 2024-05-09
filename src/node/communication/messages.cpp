#include "messages.hpp"
#include "block/chain/worksum.hpp"
#include "block/header/view.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "communication/messages.hpp"
#include "eventloop/eventloop.hpp"
#include "eventloop/types/chainstate.hpp"
#include "general/errors.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include "mempool/entry.hpp"
#include "messages_impl.hpp"
#include "message_elements/byte_size.hpp"
#include "message_elements/helper_types_impl.hpp"
#include <tuple>


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
    : MsgCombine<0,Descriptor, SignedSnapshot::Priority, Height, Worksum, Grid>(r)
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

// ForkMsg::ForkMsg(Descriptor descriptor, NonzeroHeight chainLength, Worksum worksum, NonzeroHeight forkHeight, messages::ReadRest<Grid> grid)
//     : descriptor(descriptor)
//     , chainLength(chainLength)
//     , worksum(worksum)
//     , forkHeight(forkHeight)
//     , grid(grid)
// {
//     throw_if_inconsistent(chainLength, worksum);
// }

// ForkMsg::ForkMsg(Reader& r)
//     : ForkMsg { r, r, r, r, r }
// {
// }

// ForkMsg::operator Sndbuffer() const
// {
//     // assert(grid.size() > 0); // because it is a fork
//     return gen_msg(4 + 4 + 32 + 4 + grid.raw().size())
//         << descriptor << chainLength << worksum << forkHeight
//         << grid.raw();
// }

// AppendMsg::AppendMsg(NonzeroHeight newLength,
//     Worksum worksum,
//     messages::ReadRest<Grid> grid)
//     : newLength(newLength)
//     , worksum(std::move(worksum))
//     , grid(std::move(grid))
// {
//     throw_if_inconsistent(newLength, worksum);
// }
//
// // AppendMsg::AppendMsg(Reader& r)
// //     : AppendMsg { r, r, r }
// // {
// // }
//
// AppendMsg::operator Sndbuffer() const
// {
//     return gen_msg(4 + 32 + grid.raw().size())
//         << newLength
//         << worksum
//         << grid.raw();
// }

// SignedPinRollbackMsg::SignedPinRollbackMsg(Reader& r)
//     : SignedPinRollbackMsg { r, r, r, r }
// {
// }

// SignedPinRollbackMsg::operator Sndbuffer() const
// {
//     return gen_msg(signedSnapshot.binary_size + 4 + 32 + 4)
//         << signedSnapshot
//         << shrinkLength
//         << worksum
//         << descriptor;
// }

// PingMsg::PingMsg(Reader& r)
//     : PingMsg { r, r, r, r }
// {
// }
//
// PingMsg::operator Sndbuffer() const
// {
//     return gen_msg(4 + 2 + 4 + 2 + 2)
//         << nonce
//         << sp.importance
//         << sp.height
//         << maxAddresses
//         << maxTransactions;
// }

// PongMsg::PongMsg(Reader& r)
//     : PongMsg { r, r, r }
// {
// }

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

// PongMsg::operator Sndbuffer() const
// {
//     assert(addresses.size() < std::numeric_limits<uint16_t>::max());
//     assert(txids.size() < std::numeric_limits<uint16_t>::max());
//     uint16_t nAddresses = addresses.size();
//     uint16_t nTxs = txids.size();
//
//     auto mw { gen_msg(4 + 2 + 6 * nAddresses + 2 + 18 * nTxs) };
//     mw << nonce()
//        << (uint16_t)nAddresses;
//     for (auto& v : addresses) {
//         mw << (uint32_t)v.ipv4.data
//            << (uint16_t)v.port;
//     };
//     mw << (uint16_t)nTxs;
//     for (auto& t : txids) {
//         mw << t.txid
//            << (uint16_t)t.fee.value();
//     }
//     return mw;
// }

std::string BatchreqMsg::log_str() const
{
    return "batchreq [" + std::to_string(selector().startHeight) + "," + std::to_string(selector().startHeight + selector().length - 1) + "]";
}

// BatchreqMsg::BatchreqMsg(Reader& r)
//     : BatchreqMsg { r, r }
// {
// }
//
// BatchreqMsg::operator Sndbuffer() const
// {
//     assert(selector.startHeight != 0);
//     auto& s = selector;
//     return gen_msg(4 + 4 + 4 + 2)
//         << nonce()
//         << s.descriptor
//         << s.startHeight
//         << s.length;
// }

std::string ProbereqMsg::log_str() const
{
    return "probereq " + std::to_string(descriptor().value()) + "/" + std::to_string(height());
}

// ProbereqMsg::ProbereqMsg(Reader& r)
//     : ProbereqMsg { r, r, r }
// {
// }
//
// ProbereqMsg::operator Sndbuffer() const
// {
//     return gen_msg(12) << nonce() << descriptor << height;
// }

// ProberepMsg::ProberepMsg(Reader& r)
//     : ProberepMsg { r, r, r }
// {
// }
//
// ProberepMsg::operator Sndbuffer() const
// {
//     return gen_msg(8 + currentAndRequested.byte_size())
//         << nonce() << currentDescriptor << currentAndRequested;
// }

// BatchrepMsg::BatchrepMsg(Reader& r)
//     : BatchrepMsg { r, r }
// {
// }
//
// BatchrepMsg::operator Sndbuffer() const
// {
//     return gen_msg(4 + batch.raw().size())
//         << nonce() << Range(batch.raw());
// }

std::string BlockreqMsg::log_str() const
{
    return "blockreq [" + std::to_string(range().lower) + "," + std::to_string(range().upper) + "]";
}

// BlockreqMsg::operator Sndbuffer() const
// {
//     return gen_msg(16)
//         << nonce()
//         << range;
// }
//
// BlockreqMsg::BlockreqMsg(Reader& r)
//     : BlockreqMsg { r, r }
// {
// }

// BlockrepMsg::BlockrepMsg(Reader& r)
//     : BlockrepMsg { r, r }
// {
// }
//
// BlockrepMsg::operator Sndbuffer() const
// {
//     size_t size = 0;
//     for (auto& b : blocks) {
//         size += b.serialized_size();
//     }
//     auto mw { gen_msg(4 + size) };
//     mw << nonce();
//     for (auto& b : blocks)
//         mw << b;
//
//     return mw;
// }

// TxsubscribeMsg::TxsubscribeMsg(Reader& r)
//     : TxsubscribeMsg { r, r }
// {
// }
//
// TxsubscribeMsg::operator Sndbuffer() const
// {
//     return gen_msg(8) << nonce() << upper;
// }

Sndbuffer TxnotifyMsg::direct_send(send_iter begin, send_iter end)
{
    auto mw { gen_msg(4 + (end - begin) * TransactionId::bytesize) };
    mw << RandNonce().nonce();
    for (auto iter = begin; iter != end; ++iter) {
        mw << iter->first;
    }
    return mw;
}

// TxnotifyMsg::TxnotifyMsg(Reader& r)
//     : TxnotifyMsg { r, r }
// {
// }
//
// TxnotifyMsg::operator Sndbuffer() const
// {
//     assert(txids.size() <= std::numeric_limits<uint16_t>::max());
//     assert(this->txids.size() > 0);
//     auto mw { gen_msg(4 + txids.size() * TransactionId::bytesize) };
//     mw << nonce();
//     assert(txids.size() <= std::numeric_limits<uint16_t>::max());
//     mw << (uint16_t)txids.size();
//     for (auto& t : txids) {
//         mw << t.txid
//            << t.fee;
//     }
//     return mw;
// }

TxreqMsg::TxreqMsg(Reader& r)
    : MsgCombineRequest<14,messages::VectorRest<TransactionId>>::MsgCombineRequest(r)
{
    if (txids().size() > MAXENTRIES)
        throw Error(EINV_TXREQ);
}
//
// TxreqMsg::operator Sndbuffer() const
// {
//     assert(this->txids.size() > 0);
//     auto mw { gen_msg(4 + txids.size() * TransactionId::bytesize) };
//     mw << nonce();
//     for (auto& txid : txids) {
//         mw << txid;
//     }
//     return mw;
// }

TxrepMsg::TxrepMsg(Reader& r)
    : MsgCombineReply<15, messages::VectorRest<messages::Optional<TransferTxExchangeMessage>>>(r)
{
    if (txs().size() > TxreqMsg::MAXENTRIES)
        throw Error(EINV_TXREP);
}

// TxrepMsg::operator Sndbuffer() const
// {
//     assert(this->txs.size() > 0);
//     // compute length of values;
//     size_t vals = 0;
//     size_t novals = 0;
//     for (auto& tx : txs) {
//         if (tx) {
//             vals += 1;
//         } else {
//             novals += 1;
//         }
//     }
//     size_t vals_len = vals * (1 + TransferTxExchangeMessage::bytesize);
//     size_t len = 4 + (novals * 1 + vals_len);
//
//     auto m { gen_msg(len) };
//     m << nonce();
//     for (auto& tx : txs) {
//         if (tx) {
//             m << uint8_t(1) << *tx;
//         } else {
//             m << uint8_t(0);
//         }
//     }
//     return m;
// }
// LeaderMsg::LeaderMsg(Reader& r)
//     : LeaderMsg { SignedSnapshot { r } }
// {
// }
//
// LeaderMsg::operator Sndbuffer() const
// {
//     return gen_msg(signedSnapshot.binary_size)
//         << signedSnapshot;
// }

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

// RTCRequestOffer::operator Sndbuffer() const
// {
//     return gen_msg(4 + selected.bytesize())
//         << nonce
//         << selected;
// }

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

#include <variant>

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
