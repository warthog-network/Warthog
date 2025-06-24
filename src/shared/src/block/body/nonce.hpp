#pragma once
#include "general/with_uint64.hpp"
#include <array>
#include <optional>
class Height;
class NonzeroHeight;
class PinHeight;
class Reader;
struct PinFloor;

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
    NonceReserved(Reader&r);
    static constexpr size_t byte_size(){return 3;}
    static NonceReserved zero()
    {
        return std::array<uint8_t, 3> { 0, 0, 0 };
    }
};

struct PinNonce {
    static constexpr size_t bytesize = 8;
    static constexpr size_t byte_size(){return bytesize;};

private:
    PinNonce(ReaderCheck<bytesize> r);

public:
    static std::optional<PinNonce> make_pin_nonce(NonceId, NonzeroHeight, PinHeight);
    PinNonce(Reader& r);

    [[nodiscard]] PinHeight pin_height_from_floored(PinFloor pf) const;
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
HasherSHA256& operator<<(HasherSHA256& w, const PinNonce& n);
HasherSHA256&& operator<<(HasherSHA256&& w, const PinNonce& n);
