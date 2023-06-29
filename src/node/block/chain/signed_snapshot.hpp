#pragma once
#include "block/chain/height.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
// To prevent bugs in early stage or selfish mining which could potentially cause huge rollbacks if exploited
// one leader node creates signed chain snapshots, i.e. centralized :(
// At a later stage this will be lifted
class Headerchain;
class HeaderchainSkeleton;

class Writer;
namespace chainserver {
struct Chainstate;
}
struct SignedSnapshot {
    friend class SnapshotSigner;

public:
    struct Priority {
        uint16_t importance { 0 };
        Height height { 0 };
        Priority(Reader& r);
        Priority() {};
        Priority(uint16_t i, Height h)
            : importance(i)
            , height(h) {};
        auto operator<=>(const Priority&) const = default;
    };
    struct NonzeroPriority {
        uint16_t importance;
        NonzeroHeight height;
        NonzeroPriority(Reader& r);
        NonzeroPriority(uint16_t i, NonzeroHeight h)
            : importance(i)
            , height(h) {};
        auto operator<=>(const NonzeroPriority&) const = default;
        operator Priority() const{
            return {importance,height};
        };
    };
    [[nodiscard]] bool compatible(const Headerchain& hc) const;
    [[nodiscard]] bool compatible_inefficient(const HeaderchainSkeleton& hc) const;

    static constexpr size_t binary_size { 4 + 32 + 65 };
    SignedSnapshot(Reader& r);

    auto operator<=>(const SignedSnapshot& rhs) const
    {
        return priority.operator<=>(rhs.priority);
    }
    NonzeroHeight height() const
    {
        return priority.height;
    }
    friend Writer& operator<<(Writer&, const SignedSnapshot&);

    Hash hash;
    RecoverableSignature signature;
    NonzeroPriority priority;

private:
    SignedSnapshot(NonzeroHeight height, HashView hash, RecoverableSignature signature)
        : hash(hash)
        , signature(signature)
        , priority { get_importance(), height } {};
    SignedSnapshot(NonzeroPriority p, HashView hash, RecoverableSignature signature)
        : hash(hash)
        , signature(signature)
        , priority { p }
    {
        assert(p.height != 0);
    };
    uint16_t get_importance();
};

class SnapshotSigner {
public:
    static uint16_t get_importance(const PubKey&);

    uint16_t get_importance() const { return importance; };
    SignedSnapshot sign(const chainserver::Chainstate&);
    SnapshotSigner(const PrivKey& pk)
        : pk(pk)
        , importance(get_importance(pk.pubkey())) {};

private:
    SignedSnapshot sign(NonzeroHeight, Hash);
    PrivKey pk;
    uint16_t importance;
};
