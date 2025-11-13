#pragma once
#include "crypto/hash.hpp"
#include "general/funds.hpp"
#include "general/view.hpp"
#include "id.hpp"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <string>

class Reader;
class Writer;

struct TokenFunds {
    TokenId id;
    Funds_uint64 amount;
};

class AssetName {
    static constexpr size_t maxlen { 5 };

public:
    static bool is_valid_str(std::string_view s)
    {
        auto ascii_alnum {
            [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            }
        };
        return s.size() > 0
            && s.size() <= maxlen
            && std::ranges::all_of(s, ascii_alnum);
    }

    AssetName(std::string assetName)
        : name(std::move(assetName))
    {
        if (!is_valid_str(name))
            throw Error(EASSETNAME);
    };

    std::string to_string() const
    {
        size_t i { 0 };
        for (; i < sizeof(name); ++i) {
            if (name[i] == '\0')
                break;
        }
        return { name, i };
    }
    void serialize(Serializer auto&& s) const
    {
        for (size_t i = 0; i < maxlen; ++i)
            s << uint8_t(i < name.size() ? name[i] : 0);
    }
    static constexpr size_t byte_size() { return maxlen; }

    AssetName(View<maxlen>);
    AssetName(Reader&);
    auto& c_str() const { return name; }

private:
    std::string name;
};

struct AssetBasic {
    AssetId id;
    AssetHash hash;
    AssetName name;
    AssetPrecision precision;
};
