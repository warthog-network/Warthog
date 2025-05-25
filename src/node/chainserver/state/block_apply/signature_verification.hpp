#pragma once

#include "block/body/account_id.hpp"
#include "block/body/nonce.hpp"
#include "block/body/transaction_id.hpp"
#include "block/chain/header_chain.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "general/compact_uint.hpp"
#include "general/errors.hpp"

struct VerifiedHash {
protected:
    Hash hash;

public:
    operator const Hash&() const
    {
        return hash;
    }
    VerifiedHash(Hash h, const RecoverableSignature& s, AddressView a)
        : hash(std::move(h))
    {
        if (s.recover_pubkey(hash).address() != a)
            throw Error(EINVSIG);
    }
};

struct TransactionVerifier;
struct SignerData {
    friend struct TransactionVerifier;
    SignerData(ValidAccountId id, AddressView address, PinNonce pinNonce, CompactUInt compactFee, RecoverableSignature signature)
        : origin({ id, address })
        , pinNonce(pinNonce)
        , compactFee(compactFee)
        , signature(signature)
    {
    }
    IdAddressView origin;
    PinNonce pinNonce;
    CompactUInt compactFee;
    RecoverableSignature signature;

private:
    VerifiedHash verify_hash(Hash h) const
    {
        return { h, signature, origin.address };
    }
};

struct VerifiedTransactionId : public TransactionId {
    VerifiedTransactionId(TransactionId txid, auto txIdValidator)
        : TransactionId(txid)
    {
        if (!txIdValidator(txid))
            throw Error(ENONCE);
    }
};
struct VerifiedTransaction {
    VerifiedHash hash;
    VerifiedTransactionId txid;

private:
    friend struct TransactionVerifier;
    VerifiedTransaction(VerifiedHash hash, VerifiedTransactionId txid)
        : hash(hash)
        , txid(txid)
    {
    }
};

struct TransactionVerifier {
    using validator_t = std::function<bool(TransactionId)>;
    const Headerchain& hc;
    NonzeroHeight h;
    validator_t validator;
    PinFloor pinFloor;
    TransactionVerifier(const Headerchain& hc, NonzeroHeight h, validator_t validator)
        : hc(hc)
        , h(h)
        , validator(std::move(validator))
        , pinFloor(h.pin_floor())
    {
    }

    struct PinInfo {
        PinHeight height;
        Hash hash;
    };

protected:
    PinInfo pin_info(PinNonce pinNonce) const
    {
        PinHeight pinHeight(pinNonce.pin_height_from_floored(pinFloor));
        return PinInfo {
            .height { pinHeight },
            .hash { hc.hash_at(pinHeight) }
        };
    }

public:
    template <typename... HashArgs>
    VerifiedTransaction verify(const SignerData& origin, HashArgs&&... hashArgs) const;
};

template <typename... HashArgs>
VerifiedTransaction TransactionVerifier::verify(const SignerData& sd, HashArgs&&... hashArgs) const
{
    const PinFloor pinFloor { h.pin_floor() };
    PinHeight pinHeight(sd.pinNonce.pin_height_from_floored(pinFloor));
    Hash pinHash { hc.hash_at(h) };
    return {
        sd.verify_hash((
            (HasherSHA256()
                << pinHash
                << pinHeight
                << sd.pinNonce.id
                << sd.pinNonce.reserved)
            << ... << std::forward<HashArgs>(hashArgs))),
        { { sd.origin.id, pinHeight, sd.pinNonce.id }, validator }
    };
}
