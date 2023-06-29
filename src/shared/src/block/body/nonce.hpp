#pragma once
#include "general/with_uint64.hpp"
#include <optional>
class Height;
class PinHeight;
class Reader;
class PinFloor;

template <size_t>
class ReaderCheck;
class HasherSHA256;

template <typename T>
struct Serializable {
    T value() const { return val; }

protected:
    T val;
};
struct Nonce;

class NonceId : public IsUint32 {
public:
    static NonceId random();
    explicit NonceId(Nonce);
    using IsUint32::IsUint32;
};

struct NonceReserved : public std::array<uint8_t, 3> {
    NonceReserved(const std::array<uint8_t, 3>& arr)
        : array(arr) {};
    static NonceReserved zero()
    {
        return std::array<uint8_t, 3> { 0, 0, 0 };
    }
};

struct PinNonce {
    static constexpr size_t bytesize = 8;

private:
    PinNonce(ReaderCheck<bytesize> r);

public:
    static std::optional<PinNonce> make_pin_nonce(NonceId, Height, PinHeight);
    PinNonce(Reader& r);
    PinNonce(const PinNonce&) = default;

    PinHeight pin_height(PinFloor pf) const;
    uint32_t pin_offset() const
    {
        return (relativePin << 5);
    };
    NonceId id;
    uint8_t relativePin;
    NonceReserved reserved { std::array<uint8_t, 3> { 0, 0, 0 } };

protected:
    PinNonce(NonceId id, uint8_t relativePin, NonceReserved reserved = NonceReserved::zero())
        : id(id)
        , relativePin(relativePin)
        , reserved(reserved) {};
};

Writer& operator<<(Writer& w, const PinNonce& n);
