#pragma once

#include "block/body/account_id.hpp"
#include "block/body/container.hpp"
#include "block/chain/height.hpp"
#include "block/chain/history/index.hpp"
#include "block/chain/worksum.hpp"
#include "block/header/header.hpp"
#include "block/id.hpp"
#include "crypto/address.hpp"
#include "crypto/hash.hpp"
#include "db/sqlite_fwd.hpp"
#include "defi/token/id.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include "general/funds.hpp"
#include <cstring>
#include <limits>

namespace sqlite {
class ColumnConverter {
    const Column& c;

    template <size_t size>
    std::array<uint8_t, size> get_array() const
    {
        std::array<uint8_t, size> res;
        if (c.getBytes() != size)
            throw std::runtime_error(
                "Database corrupted, cannot load " + std::to_string(size) + " bytes");
        memcpy(res.data(), c.getBlob(), size);
        return res;
    }

    std::vector<uint8_t> get_vector() const
    {
        std::vector<uint8_t> res(c.getBytes());
        memcpy(res.data(), c.getBlob(), c.getBytes());
        return res;
    }

public:
    int64_t getInt64() const noexcept { return c.getInt64(); }
    uint64_t getUInt64() const
    {
        auto i { getInt64() };
        if (i < 0) {
            throw std::runtime_error("Database might be corrupted. Expected non-negative value.");
        }
        return i;
    }

    uint64_t getUInt32() const
    {
        auto i { getUInt64() };
        if (i > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("Database might be corrupted. Value overflows uint32_t.");
        return i;
    }
    ColumnConverter(const Column& c)
        : c(c)
    {
    }

    operator Hash() const { return { get_array<32>() }; }
    operator Height() const { return Height(getUInt32()); }
    operator HistoryId() const { return HistoryId { getUInt64() }; }
    operator std::vector<uint8_t>() const { return get_vector(); }
    operator Address() const { return get_array<20>(); }
    operator BodyContainer() const { return get_vector(); }
    operator Header() const { return get_array<80>(); }
    operator NonzeroHeight() const
    {
        return Height { *this }.nonzero_throw("NonzeroHeight cannot be 0.");
    }
    operator int64_t() const { return getInt64(); }
    operator AccountId() const { return AccountId { getUInt64() }; }
    operator IsUint64() const { return IsUint64(getUInt64()); }
    operator BlockId() const { return BlockId(getInt64()); }
    operator Funds_uint64() const
    {
        auto v { Funds_uint64::from_value(getUInt64()) };
        if (!v.has_value())
            throw std::runtime_error("Database corrupted, invalid funds");
        return *v;
    }
    operator uint64_t() const { return getUInt64(); }
    operator TokenHash() const { return Hash(*this); }
    operator TokenName() const
    {
        return TokenName::parse_throw(static_cast<std::string>(c));
    }
    operator Price_uint64() const
    {
        auto p { Price_uint64::from_uint32(getUInt32()) };
        if (!p)
            throw std::runtime_error("Cannot parse price");
        return *p;
    }
    operator Worksum() const { return get_array<32>(); }
    operator TokenId() const { return TokenId(getUInt32()); }
};

namespace bind_convert {
    template <size_t N>
    inline auto convert(const std::array<uint8_t, N>& v) { return std::span(v); }
    template <size_t N>
    inline auto convert(const View<N>& v) { return v.span(); }
    inline auto convert(const Worksum& ws) { return ws.to_bytes(); }
    inline auto convert(const std::vector<uint8_t>& v) { return std::span(v); }
    inline auto convert(Funds_uint64 f) { return (int64_t)f.value(); }
    inline auto convert(Wart f) { return (int64_t)f.E8(); }
    inline auto convert(int64_t i) { return i; }
    inline auto convert(uint64_t i) { return (int64_t)i; }
    inline auto convert(IsUint64 i) { return i.value(); }
    inline auto convert(IsUint32 i) { return (int64_t)i.value(); }
    inline const auto& convert(const std::string& s) { return s; }
}
}
