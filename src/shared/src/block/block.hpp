#pragma once
#include "block/body/container.hpp"
#include "block/body/view.hpp"
#include "block/chain/height.hpp"
#include "block/header/header.hpp"
#include <optional>

struct TransactionId;

class ParsedBody : public ParsableBodyContainer {
private:
    ParsedBody(BodyContainer, NonzeroHeight, HeaderView);

public:
    [[nodiscard]] static ParsedBody create_throw(NonzeroHeight, HeaderView, BodyContainer);
    [[nodiscard]] BodyView view() const;
    BodyStructure structure;
};

struct ParsedBlock;
struct Block {
    NonzeroHeight height;
    Header header;
    BodyContainer body;
    [[nodiscard]] ParsedBlock parse_throw() &&;
    bool operator==(const Block&) const = default;
    operator bool() { return body.size() > 0; }
};

struct ParsedBlock {
    NonzeroHeight height;
    Header header;
    ParsedBody body;
    [[nodiscard]] static ParsedBody create_throw(NonzeroHeight, HeaderView, BodyContainer);

private:
    ParsedBlock(NonzeroHeight h, HeaderView v, BodyContainer bc);
    friend struct Block;

public:
    std::vector<TransactionId> read_tx_ids();
};
