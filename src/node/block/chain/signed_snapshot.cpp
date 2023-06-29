#include "signed_snapshot.hpp"
#include "block/chain/header_chain.hpp"
#include "chainserver/state/state.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <array>
#include <string_view>

SignedSnapshot::Priority::Priority(Reader& r)
    : Priority({r.uint16(), Height(r)}) {};
uint16_t SignedSnapshot::get_importance()
{
    return SnapshotSigner::get_importance(signature.recover_pubkey(hash));
};

bool SignedSnapshot::compatible(const Headerchain& hc) const
{
    return (hc.length() < priority.height) || hc.hash_at(priority.height) == hash;
};

bool SignedSnapshot::compatible_inefficient(const HeaderchainSkeleton& hc) const
{
    return (hc.length() < priority.height) || hc.inefficient_get_header(priority.height).value().hash() == hash;
};


SignedSnapshot::SignedSnapshot(Reader& r)
    : SignedSnapshot({ NonzeroHeight(r.uint32()), r.view<HashView>(), r.view<65>() }) {};

Writer& operator<<(Writer& w, const SignedSnapshot& sp)
{
    return w
        << sp.priority.height
        << sp.hash
        << sp.signature;
};

uint16_t SnapshotSigner::get_importance(const PubKey& pk)
{
    auto hex { pk.to_string() };

    using namespace std::literals;
    std::array leaderPubkeys {
        "02686b3ea29dc1fcfd0de6fd067d1903f09080e2407d1f230e9551b69fbd808408"sv,
        "03ac7ab39fc4ad459170470f1dc1d85fe7cb92d987d75467f3fa5d6692cd27cebf"sv,
        "035bd7cec5091b482f1b89db677abe94e247e1fbf5dcfb3cd883f45201791f1748"sv,
        "0251fe6976096039ad68d9f987cc4d924e43ad9c7e1080301f4fe3b53868f69b87"sv,
        "02b83c2b7580983382bd29ba138f20d84f3172d1cad3103eabe50ebad3fde02011"sv,
        "02f581bd9ca28a19802ceb2dc96ba53b1f3e7dcbbb06bb7f97032843518d10dbcf"sv,
        "03b612b5bb4648cedc65080452418486c13249d21c30ff2fc08322012e1196d868"sv,
        "0283df9d39202c98d3c40402d23662d3d77c30b48ec171ba8cd39b77ff2b89ff71"sv
    };
    auto pos = std::ranges::find(leaderPubkeys, hex);
    if (pos == leaderPubkeys.end())
        throw Error(EBADLEADER);
    return pos - leaderPubkeys.begin();
};

SignedSnapshot SnapshotSigner::sign(const chainserver::Chainstate& cs)
{
    auto l { cs.length() };
    assert(cs.final_hash() == cs.headers().get_hash(l));
    assert(l != 0);
    return sign(l.nonzero_assert(), cs.final_hash());
}

SignedSnapshot SnapshotSigner::sign(NonzeroHeight h, Hash hash)
{
    return { SignedSnapshot::NonzeroPriority { importance, h }, hash, pk.sign(hash) };
};
