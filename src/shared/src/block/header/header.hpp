#pragma once
#include "block/header/view.hpp"
#include <array>
#include <cstdint>

class Height;
constexpr size_t HEADERBYTELENGTH = 80;
class Header : public std::array<uint8_t, HEADERBYTELENGTH> {
public:
    using array::array;
    using array::operator=;
    using array::data;
    using array::size;
    Header() {};
    Header(const char*);
    Header(const std::array<uint8_t, 80>& arr)
        : array(arr) {};
    Header(HeaderView hv)
    {
        memcpy(data(), hv.data(), 80);
    }
    Header& operator=(HeaderView hv) { return *this = Header(hv); }
    operator HeaderView() const { return HeaderView(data()); }
    Target target(NonzeroHeight) const;
    inline bool validPOW(const Hash&, NonzeroHeight) const;
    inline HashView prevhash() const;
    inline HashView merkleroot() const;
    void set_merkleroot(std::array<uint8_t, 32>);
    void set_nonce(uint32_t nonce);
    inline uint32_t timestamp() const;
    inline uint32_t version() const;
    inline TargetV1 target_v1() const;
    inline TargetV2 target_v2() const;
    inline uint32_t nonce() const;
    inline Hash hash() const;
    constexpr static size_t byte_size()
    {
        return HEADERBYTELENGTH;
    }
};
