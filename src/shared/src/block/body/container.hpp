#pragma once
#include "block/body/view_fwd.hpp"
#include "block/chain/height.hpp"
#include "block/version.hpp"
#include <cstdint>
#include <span>
#include <vector>

class Reader;
class BlockVersion;
class Writer;
namespace block {
namespace body {

class Container {
public:
    Container(std::span<const uint8_t>);
    Container(std::vector<uint8_t> bytes)
        : bytes(std::move(bytes))
    {
    }
    Container(Reader& r);
    friend Writer& operator<<(Writer&, const Container&);
    size_t byte_size() const { return size() + 4; }
    size_t size() const { return bytes.size(); }
    auto& data() const { return bytes; }
    auto& data() { return bytes; }
    operator std::span<const uint8_t>() const { return bytes; }
    [[nodiscard]] Structure parse_structure_throw(NonzeroHeight h, BlockVersion v) const;
    bool operator==(const Container&) const = default;

private:
    std::vector<uint8_t> bytes;
};

class VersionedContainer : public Container {
public:
    VersionedContainer(Container c, BlockVersion v)
        : Container(std::move(c))
        , version(v)
    {
    }
    BlockVersion version;
};

class ParsableBodyContainer : public Container {
private:
    friend class ParsedBody;
    ParsableBodyContainer(Container bc)
        : Container(std::move(bc))
    {
    }
};

}
}

using BodyContainer = block::body::Container;
using VersionedBodyContainer = block::body::VersionedContainer;
