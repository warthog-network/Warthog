#include "block.hpp"
#include "block/body/transaction_id.hpp"
#include "block/body/view.hpp"
#include "block/header/header_impl.hpp"

namespace block {

ParsedBlock Block::parse_throw() &&
{
    return { height, header, std::move(body) };
}

namespace body {
ParsedBody::ParsedBody(NonzeroHeight height, HeaderView header, body::Container bc)
    : ParsableBodyContainer(std::move(bc))
    , structure(Container::parse_structure_throw(height, header.version()))
{
    if (header.merkleroot() != view().merkle_root(height))
        throw Error(EMROOT);
}

ParsedBody ParsedBody::create_throw(NonzeroHeight h, HeaderView hv, body::Container bc)
{
    return { h, hv, std::move(bc) };
}

BodyView ParsedBody::view() const
{
    return { *this, structure };
}

}
ParsedBlock::ParsedBlock(NonzeroHeight h, HeaderView v, body::Container bc)
    : height(h)
    , header(v)
    , body(body::ParsedBody::create_throw(height, header, std::move(bc)))
{
}

ParsedBlock ParsedBlock::create_throw(NonzeroHeight h, HeaderView hv, body::Container b)
{
    return ParsedBlock { h, hv, std::move(b) };
}

std::vector<TransactionId> ParsedBlock::read_tx_ids()
{
    auto bv { body.view() };
    PinFloor pinFloor { height.pin_floor() };

    std::vector<TransactionId> out;
    for (auto t : bv.wart_transfers()) {
        auto txid { t.txid(t.pin_height(pinFloor)) };
        out.push_back(txid);
    }
    return out;
}
}
