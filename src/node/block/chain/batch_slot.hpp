#pragma once
#include "block/chain/height.hpp"

struct Batchslot {
    explicit Batchslot(uint32_t i)
        : i(i)
    {
    }
    explicit Batchslot(Height h)
        : Batchslot((h.val - 1) / HEADERBATCHSIZE)
    {
    }
    [[nodiscard]] Height offset() const
    {
        return Height(i * HEADERBATCHSIZE);
    }
    [[nodiscard]] NonzeroHeight lower() const
    {
        return (i * HEADERBATCHSIZE + 1);
    }
    [[nodiscard]] NonzeroHeight upper() const
    {
        return (i + 1) * HEADERBATCHSIZE;
    }
    [[nodiscard]] Batchslot operator+(uint32_t add) const{
        return Batchslot(i+add);
    }
    [[nodiscard]] uint32_t operator-(Batchslot s) const{
        return i-s.i;
    }
    Batchslot& operator++()
    {
        i += 1;
        return *this;
    }
    size_t index() { return i; }
    [[nodiscard]] Batchslot operator-(uint32_t j){
        assert(i >= j );
        return Batchslot(i-j);
    }

    friend bool operator==(const Batchslot&, const Batchslot&) = default;

    auto operator<=>(const Batchslot& h2) const = default;
private:
    uint32_t i;
};
