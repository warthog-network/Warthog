#pragma once
#include "general/funds.hpp"
#include "general/params.hpp"
#include "general/view.hpp"
#include "general/with_uint64.hpp"
#include "id.hpp"
#include <cassert>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>

class Reader;
class Writer;

constexpr auto DefaultTokenSupply {Funds::from_value_throw( (100000000 * COINUNIT))};

enum class TokenMintType { Default = 0,
    Auction = 1 };

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
    TokenName(View<5>);
    TokenName(Reader&);
    auto& c_str() const { return name; }
    friend Writer& operator<<(Writer&, const TokenName&);

private:
    char name[6] = { '\0', '\0', '\0', '\0', '\0', '\0' };
};
