#pragma once
#include "block/body/elements.hpp"
#include "block/version.hpp"
#include "general/reader_declaration.hpp"

namespace block {
namespace body {
template <typename T>
struct body_vector : public std::vector<T> {
    using std::vector<T>::vector;
    body_vector(std::vector<T> v)
        : std::vector<T>(std::move(v))
    {
    }
    template <typename uint_t>
    Writer& write(Writer& w) const
    {
        w << uint_t(this->size());
        return write_elements(w);
    }
    Writer& write_elements(Writer& w) const
    {
        for (auto& e : *this)
            w << e;
        return w;
    }
    auto operator<=>(const body_vector<T>&) const = default;

    size_t byte_size() const;
    body_vector(size_t n, Reader& r);
};

struct TokenSection {
private:
    AssetId id;

public:
    body_vector<body::TokenTransfer> assetTransfers;
    body_vector<body::TokenTransfer> shareTransfers;
    body_vector<body::Order> orders;
    body_vector<body::LiquidityDeposit> liquidityAdd;
    body_vector<body::LiquidityWithdraw> liquidityRemove;

    static constexpr const size_t n_vectors = 5;
    void append_tx_ids(PinFloor, std::vector<TransactionId>& appendTo) const;
    auto asset_id() const { return id; }
    auto share_id() const { return id.share_id(); }

    Writer& write(Writer&);
    TokenSection(Reader&);
    TokenSection(AssetId tid)
        : id(tid) { };
    size_t byte_size() const;
};
}

class Body {
private:
    Body(Reader&, NonzeroHeight h, BlockVersion v);
    Body(std::vector<Address> newAddresses, body::Reward reward)
        : newAddresses(std::move(newAddresses))
        , reward(std::move(reward))
    {
    }

    template <typename T>
    using body_vector = body::body_vector<T>;

public:
    std::vector<TransactionId> tx_ids(NonzeroHeight) const;
    static Body parse_throw(std::span<const uint8_t> rd, NonzeroHeight h, BlockVersion version);
    size_t byte_size() const;
    std::vector<uint8_t> serialize() const;
    Body(std::span<const uint8_t> data, BlockVersion v, NonzeroHeight h);
    body_vector<Address> newAddresses;
    body::Reward reward;
    body_vector<body::WartTransfer> wartTransfers;
    body_vector<body::Cancelation> cancelations;
    body_vector<body::TokenSection> tokens;
    body_vector<body::AssetCreation> tokenCreations;
};

}
