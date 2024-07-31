#pragma once
#include "general/view.hpp"
#include "general/with_uint64.hpp"
#include <cassert>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
struct TokenId {
    bool operator==(const TokenId&) const = default;
    auto operator<=>(const TokenId&) const = default;
    int64_t value;
};

class Reader;
class Writer;
class TokenName {
    TokenName(std::string s)
    {
        memcpy(name, s.c_str(), s.size());
    }

public:
    static std::optional<TokenName> from_string(std::string s)
    {
        if (s.size() > 5)
            return {};
        for (auto c : s) {
            if (!(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9'))
                return {};
        }
        return TokenName { std::move(s) };
    }
    View<5> view() const { return View<5>(reinterpret_cast<const uint8_t*>(name)); }
    static constexpr size_t byte_size() { return 5; }
    TokenName(const uint8_t*);
    auto& c_str() const { return name; }
    friend Writer& operator<<(Writer&, const TokenName&);

private:
    char name[6] = { '\0', '\0', '\0', '\0', '\0', '\0' };
};

struct TokenIndex {

    TokenIndex(uint32_t data)
        : data(data)
    {
        assert((data & 0x80000000) == 0);
    }
    auto value() const { return data; }

private:
    uint32_t data;
};

struct TokenCreationCode : public IsUint32 {
public:
    using variant_t = std::variant<TokenIndex>;

private:
    variant_t parse() const
    {
        if ((val & 0x80000000) == 0) {
            return TokenIndex { val };
        }
        return TokenIndex { 0x7FFFFFFF & val }; // TODO do something else here
    }

public:
    TokenCreationCode(uint32_t data)
        : IsUint32(data)
    {
    }
};
