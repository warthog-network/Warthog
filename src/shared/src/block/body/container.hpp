#pragma once
#include "block/chain/height.hpp"
#include <cstdint>
#include <span>
#include <vector>

class Reader;
class BlockVersion;
class Writer;
class BodyStructure;
class BodyContainer {
public:
    BodyContainer(std::span<const uint8_t>);
    BodyContainer(std::vector<uint8_t> bytes)
        : bytes(std::move(bytes))
    {
    }
    BodyContainer(Reader& r);
    friend Writer& operator<<(Writer&, const BodyContainer&);
    size_t byte_size() const { return size() + 4; }
    size_t size() const { return bytes.size(); }
    auto& data() const { return bytes; }
    auto& data() { return bytes; }
    [[nodiscard]] std::optional<BodyStructure> parse_structure(NonzeroHeight h, BlockVersion v) const;
    [[nodiscard]] BodyStructure parse_structure_throw(NonzeroHeight h, BlockVersion v) const;
    bool operator==(const BodyContainer&) const = default;

private:
    std::vector<uint8_t> bytes;
};

class ParsableBodyContainer : public BodyContainer {
private:
    friend class ParsedBody;
    ParsableBodyContainer(BodyContainer bc)
        : BodyContainer(std::move(bc))
    {
    }
};
