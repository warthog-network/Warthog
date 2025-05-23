#pragma once
#include "block/body/container.hpp"
#include "block/body/view.hpp"
#include "block/chain/height.hpp"
#include "block/header/header.hpp"
#include "block_fwd.hpp"

struct TransactionId;

namespace block {
namespace body {
class ParsedBody : public ParsableBodyContainer {
private:
    ParsedBody(NonzeroHeight, HeaderView, Container);

public:
    [[nodiscard]] static ParsedBody create_throw(NonzeroHeight, HeaderView, Container);
    [[nodiscard]] BodyView view() const;
    Structure structure;
};
}

struct ParsedBlock;
struct Block {
    NonzeroHeight height;
    Header header;
    block::body::Container body;
    [[nodiscard]] ParsedBlock parse_throw() &&;
    bool operator==(const Block&) const = default;
    operator bool() { return body.size() > 0; }
};

struct ParsedBlock {
    NonzeroHeight height;
    Header header;
    block::body::ParsedBody body;
    [[nodiscard]] static ParsedBlock create_throw(NonzeroHeight, HeaderView, body::Container);

private:
    ParsedBlock(NonzeroHeight h, HeaderView v, body::Container bc);
    friend struct Block;

public:
    std::vector<TransactionId> read_tx_ids();
};
}
