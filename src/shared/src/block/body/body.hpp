#pragma once
#include "block/body/body_fwd.hpp"
#include "block/body/container.hpp"
#include "block/body/elements.hpp"
#include "block/version.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/reader_declaration.hpp"
#include "general/writer_fwd.hpp"
#include <cstdint>

namespace block {
namespace body {
struct MerkleTree {
    void add_hash(Hash hash)
    {
        hashes.push_back(std::move(hash));
    }
    void root() { }
    std::vector<Hash> hashes;
};

struct MerkleReadHooker;
struct MerkleReadHook {
public:
    operator Reader&()
    {
        return reader;
    }

    MerkleReadHook(MerkleReadHook&& mi)
        : MerkleReadHook(mi.reader, mi.creator)
    {
        mi.begin = nullptr;
    }

    ~MerkleReadHook();

private:
    friend MerkleReadHooker;
    MerkleReadHook(Reader& r, MerkleReadHooker& c)
        : reader(r)
        , creator(c)
        , begin(r.cursor())
    {
    }
    MerkleReadHook(const MerkleReadHook& mi) = delete;

private:
    Reader& reader;
    MerkleReadHooker& creator;
    const uint8_t* begin;
};

struct MerkleReadHooker {
    friend MerkleReadHook;

    MerkleReadHooker(Reader& r, MerkleTree* tree)
        : reader(r)
        , tree(tree)
    {
    }

    MerkleReadHook hook()
    {
        return MerkleReadHook(reader, *this);
    }

private: // methods
    MerkleReadHooker(const MerkleReadHooker&) = delete;

    void add_hash_of(const std::span<const uint8_t>& s)
    {
        if (tree)
            tree->add_hash(hashSHA256(s));
    }

public:
    Reader& reader;

private:
    MerkleTree* tree;
};

inline MerkleReadHook::~MerkleReadHook()
{
    if (begin)
        std::span<const uint8_t>(begin, reader.cursor());
}

struct MerkleWriteHooker;
struct MerkleWriteHook {
public:
    operator Writer&()
    {
        return writer;
    }

    MerkleWriteHook(MerkleWriteHook&& mi)
        : MerkleWriteHook(mi.writer, mi.creator)
    {
        mi.begin = nullptr;
    }

    ~MerkleWriteHook();

private:
    friend MerkleWriteHooker;
    MerkleWriteHook(Writer& w, MerkleWriteHooker& c);
    MerkleWriteHook(const MerkleWriteHook& mi) = delete;

public:
    Writer& writer;

private:
    MerkleWriteHooker& creator;
    const uint8_t* begin;
};
struct MerkleWriteHooker {
    MerkleWriteHooker(Writer& w, MerkleTree* tree)
        : writer(w)
        , tree(tree)
    {
    }
    [[nodiscard]] MerkleWriteHook hook();

private:
    void add_hash_of(const std::span<const uint8_t>& s)
    {
        if (tree)
            tree->add_hash(hashSHA256(s));
    }

public:
    Writer& writer;

private:
    MerkleTree* tree;
};
template <typename T>
struct body_vector : public std::vector<T> {
    using std::vector<T>::vector;
    body_vector(std::vector<T> v)
        : std::vector<T>(std::move(v))
    {
    }
    void serialize(Serializer auto&& s)
    {
        for (auto& e : *this)
            s << e;
    }

    template <typename uint_t>
    void write(MerkleWriteHooker& m) const
    {
        m.writer << T(this->size());
        for (auto& e : *this) {
            auto h { m.hook() };
            h.writer << e;
        }
    }

    void append_txids(std::vector<TransactionId>& v, PinFloor pf) const
    {
        for (auto& e : *this)
            e.append_txids(v, pf);
    }

    auto operator<=>(const body_vector<T>&) const = default;

    size_t byte_size() const
    {
        size_t i { 0 };
        for (auto& elem : *this)
            i += elem.byte_size();
        return i;
    }
    body_vector(size_t n, MerkleReadHooker& m)
    {
        for (size_t i { 0 }; i < n; ++i)
            this->push_back({ m.hook() });
    }

    body_vector(size_t n, Reader& r)
    {
        for (size_t i { 0 }; i < n; ++i)
            this->push_back({ r });
    }
};

class NewAddresses {
public:
    NewAddresses(AccountId nextAccountId)
        : nextAccountId(nextAccountId)
    {
    }
    AccountId operator[](const Address& address)
    {
        auto [iter, inserted] { map.try_emplace(address, nextAccountId) };
        if (inserted)
            nextAccountId++;
        return iter->second;
    }
    std::vector<Address> get_vector() const
    {
        std::vector<Address> out;
        for (auto& [addr, _id] : map)
            out.push_back(addr);
        return out;
    };

private:
    std::map<Address, AccountId> map;
    AccountId nextAccountId;
};
namespace elements {

template <typename Elem>
struct VectorElement {
    auto& get() const { return elements; }
    auto& get() { return elements; }

    [[nodiscard]] size_t byte_size() const
    {
        return elements.byte_size();
    }
    void serialize(Serializer auto&& s) const
    {
        s << elements;
    }
    void append_txids(std::vector<TransactionId>& out, PinFloor pf) const
    {
        elements.append_txids(out, pf);
    }

    VectorElement() { }
    VectorElement(size_t N, MerkleReadHooker& m)
        : elements(N, m)
    {
    }

protected:
    body_vector<Elem> elements;
};

namespace tokens {

struct AssetTransfers : public VectorElement<body::TokenTransfer> {
    using VectorElement::VectorElement;
    auto& asset_transfers() const { return get(); }
    auto& asset_transfers() { return get(); }
};
struct ShareTransfers : public VectorElement<body::TokenTransfer> {
    using VectorElement::VectorElement;
    auto& share_transfers() const { return get(); }
    auto& share_transfers() { return get(); }
};
struct Orders : public VectorElement<body::Order> {
    using VectorElement::VectorElement;
    auto& orders() const { return get(); }
    auto& orders() { return get(); }
};
struct LiquidityDeposits : public VectorElement<body::LiquidityDeposit> {
    using VectorElement::VectorElement;
    auto& liquidity_deposits() const { return get(); }
    auto& liquidity_deposits() { return get(); }
};
struct LiquidityWithdrawals : public VectorElement<body::LiquidityWithdraw> {
    using VectorElement::VectorElement;
    auto& liquidity_withdrawals() const { return get(); }
    auto& liquidity_withdrawals() { return get(); }
};

template <size_t N>
struct TenBitsBlocks {
private:
    using arr_t = std::array<size_t, N>;
    static constexpr size_t byte_size() { return (10 * N + 7) / 8; }
    template <size_t... Is>
    static auto read_data(std::index_sequence<Is...>, Reader& rd)
    {
        auto r10 { [bits = size_t(0), nbits = 0u, &rd]() mutable -> size_t {
            while (nbits < 10) {
                bits <<= 8;
                bits |= rd.uint8();
                nbits += 8;
            }
            auto excess { nbits - 10 };
            auto res { bits >> excess };
            bits &= ((1 << excess) - 1);
            nbits -= 10;
            return res;
        } };
        return arr_t { (void(Is), r10())... };
    }

public:
    TenBitsBlocks(Reader& rd)
        : data(read_data(std::make_index_sequence<N>(), rd))
    {
    }
    template <typename... Ts>
    requires(sizeof...(Ts) == N)
    TenBitsBlocks(arr_t arr)
        : data { std::move(arr) }
    {
    }
    void serialize(Serializer auto&& s)
    {
        uint32_t bits { 0 };
        size_t nbits { 0 };
        for (auto v : this->data) {
            bits |= uint32_t(v) << (22 - nbits);
            nbits += 10;
            while (nbits >= 8) {
                s << (uint8_t)(bits >> 24);
                bits <<= 8;
                nbits -= 8;
            }
        }
        if (nbits > 0)
            s << uint8_t(bits >> 24);
        return s;
    }
    template <size_t i>
    requires(i < N)
    size_t at() const
    {
        return data[i];
    }

private:
    arr_t data;
};

template <typename... Ts>
struct TokenEntries : public Ts... {
private:
    using bits_t = TenBitsBlocks<sizeof...(Ts)>;
    template <size_t... Is>
    TokenEntries(std::integer_sequence<size_t, Is...>, const bits_t& lengths, Reader& r)
        : Ts(lengths.at(Is), r)...
    {
    }

public:
    TokenEntries(Reader& r)
        : TokenEntries(std::index_sequence_for<Ts...>(), bits_t(r), r)
    {
    }
    auto& token_entries() const { return *this; }
    [[nodiscard]] size_t byte_size() const
    {
        return (static_cast<const Ts*>(this)->byte_size() + ...);
    }
    void serialize(Serializer auto&& s) const
    {
        s << bits_t { static_cast<const Ts*>(this)->get().size()... };
        (s << ... << static_cast<const Ts*>(this));
    }
    void append_txids(std::vector<TransactionId>& v, PinFloor pf) const
    {
        (static_cast<const Ts*>(this)->append_txids(v, pf), ...);
    }
    void write(MerkleWriteHooker& m) const
    {
        auto write_hooked { [&](auto& arg) {
            auto h { m.hook() };
            h.writer << arg;
        } };
        (hook_arg(*static_cast<const Ts*>(this)), ...);
    }

    TokenEntries() { }
};

struct AssetIdElement {
protected:
    AssetId assetId;

public:
    AssetIdElement(AssetId id)
        : assetId(id)
    {
    }
    auto asset_id() const { return assetId; }
    auto share_id() const { return assetId.share_id(); }
};

struct TokenSection : public AssetIdElement, public TokenEntries<AssetTransfers, ShareTransfers, Orders, LiquidityDeposits, LiquidityWithdrawals> {

public:
    // body_vector<body::TokenTransfer> assetTransfers;
    // body_vector<body::TokenTransfer> shareTransfers;
    // body_vector<body::Order> orders;
    // body_vector<body::LiquidityDeposit> liquidityAdd;
    // body_vector<body::LiquidityWithdraw> liquidityRemove;

    static constexpr const size_t n_vectors = 5;
    void append_tx_ids(PinFloor, std::vector<TransactionId>& appendTo) const;

    void write(MerkleWriteHooker& w);
    TokenSection(Reader&);
    TokenSection(AssetId tid)
        : AssetIdElement(tid) { };
    void append_txids(std::vector<TransactionId>& out, PinFloor pf) const
    {
        TokenEntries::append_txids(out, pf);
    }
    size_t byte_size() const
    {
        return assetId.byte_size() + TokenEntries::byte_size();
    };
};
}

template <typename UInt, typename Elem>
struct SizeVector : public VectorElement<Elem> {
    [[nodiscard]] size_t byte_size() const { return sizeof(UInt) + this->get().byte_size(); }
    void write(MerkleWriteHooker& w) const
    {
        this->elements.template write<UInt>(w);
    }
    SizeVector() { }
    SizeVector(MerkleReadHooker& m)
        : VectorElement<Elem> { m.reader.remaining() > 0 ? size_t(UInt(m.reader)) : 0, m }
    {
    }
};

struct WartTransfers : public SizeVector<uint32_t, body::WartTransfer> {
    auto& wart_transfers() const { return get(); }
    auto& wart_transfers() { return get(); }
};

struct Cancelations : public SizeVector<uint16_t, body::Cancelation> {
    auto& cancelations() const { return get(); }
    auto& cancelations() { return get(); }
};

struct TokenSections : public SizeVector<uint16_t, tokens::TokenSection> {
    auto& tokens() const { return get(); }
    auto& tokens() { return get(); }
};

struct AssetCreations : public SizeVector<uint16_t, body::AssetCreation> {
    auto& asset_creations() const { return get(); }
    auto& asset_creations() { return get(); }
};

template <typename... Ts>
struct CombineElements : public Ts... {
    CombineElements(MerkleReadHooker& r)
        : Ts(r)...
    {
    }
    CombineElements() { }

    [[nodiscard]] size_t byte_size() const
    {
        return (static_cast<const Ts*>(this)->byte_size() + ...);
    }
    // void serialize(Serializer auto&& s) const
    // {
    //     (s << ... << static_cast<const Ts*>(this));
    // }
    void write(MerkleWriteHooker& m) const
    {
        (static_cast<const Ts*>(this)->write(m), ...);
    }
    void append_txids(std::vector<TransactionId>& v, PinFloor pf) const
    {
        (static_cast<const Ts*>(this)->append_txids(v, pf), ...);
    }
};

struct Entries : public CombineElements<WartTransfers, Cancelations, TokenSections, AssetCreations> {
    using CombineElements::CombineElements;
    Entries& entries() { return *this; }
    const Entries& entries() const { return *this; }
    void validate_version(BlockVersion version)
    {
        if (version.value() < 4) {
            if (!cancelations().empty() || !tokens().empty() || !asset_creations().empty()) {
                throw Error(EBLOCKV4);
            }
        };
    }
};
}
struct SerializedBody {
    BodyContainer container;
    MerkleTree merkleTree;
    std::vector<uint8_t> merkle_prefix() const;
    Hash merkle_root(Height h) const;
};

struct AddressReward {
    AddressReward(std::vector<Address> newAddresses, body::Reward reward)
        : newAddresses(std::move(newAddresses))
        , reward(std::move(reward))
    {
    }
    body_vector<Address> newAddresses;
    body::Reward reward;
};
using elements::Entries;
using elements::tokens::TokenSection;

class Body : public AddressReward, public Entries {
private:
    template <typename T>
    using body_vector = body_vector<T>;

public:
    Body(std::vector<Address> newAddresses, Reward reward, Entries entries)
        : AddressReward(std::move(newAddresses), std::move(reward))
        , Entries(std::move(entries))
    {
    }
    std::vector<TransactionId> tx_ids(NonzeroHeight) const;
    static Body parse_throw(std::span<const uint8_t> rd, NonzeroHeight h, BlockVersion version, MerkleTree* ptree);
    size_t byte_size() const;
    SerializedBody serialize() const;
    Body(std::span<const uint8_t> data, BlockVersion v, NonzeroHeight h, MerkleTree*);
};
}

}
