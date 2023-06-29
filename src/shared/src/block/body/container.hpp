#pragma once
#include <cstdint>
#include <span>
#include <vector>

class Reader;
class Writer;
class BodyView;
class BodyContainer {
public:
    BodyContainer(std::span<const uint8_t>);
    BodyContainer(std::vector<uint8_t> bytes):bytes(std::move(bytes)){};
    BodyContainer(Reader& r);
    friend Writer& operator<<(Writer&, const BodyContainer&);
    size_t serialized_size() const { return size() + 4; }
    size_t size() const { return bytes.size(); }
    auto& data() const{return bytes;}
    auto& data(){return bytes;}
    BodyView view() const;
    bool operator==(const BodyContainer&) const = default;

private:
    std::vector<uint8_t> bytes;
};
