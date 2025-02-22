#include "block.hpp"
#include "block/body/parse.hpp"
#include "block/body/transaction_id.hpp"
#include "block/body/view.hpp"
#include "block/header/header_impl.hpp"

ParsedBlock Block::parse_throw() &&
{
    return { height, header, std::move(body) };
}

ParsedBody::ParsedBody(NonzeroHeight height, HeaderView header, BodyContainer bc)
    : ParsableBodyContainer(std::move(bc))
    , structure(BodyContainer::parse_structure_throw(height, header.version()))
{
    if (header.merkleroot() != view().merkle_root(height))
        throw Error(EMROOT);
}

ParsedBody ParsedBody::create_throw(NonzeroHeight h, HeaderView v, BodyContainer bc)
{
    return { std::move(bc), h, v };
}

BodyView ParsedBody::view() const
{
    return { *this, structure };
}

ParsedBlock::ParsedBlock(NonzeroHeight h, HeaderView v, BodyContainer bc)
    : height(h)
    , header(v)
    , body(ParsedBody::create_throw(height, header, std::move(bc)))
{
}

std::vector<TransactionId> ParsedBlock::read_tx_ids()
{
    auto bv { body.view() };
    PinFloor pinFloor { PrevHeight(height) };

    std::vector<TransactionId> out;
    for (auto t : bv.transfers()) {
        auto txid { t.txid(t.pinHeight(pinFloor)) };
        out.push_back(txid);
    }
    return out;
}
