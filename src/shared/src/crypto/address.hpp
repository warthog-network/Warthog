#pragma once
#include "general/view.hpp"
#include <array>
#include <string>

class AddressView : public View<20> {
public:
    explicit AddressView(const uint8_t* pos)
        : View(pos) {};
    std::array<uint8_t, 24> serialize() const;
    std::string to_string() const;
};

class Address : public std::array<uint8_t, 20> {
public:
    friend class PubKey;
    Address(const std::string_view);
    Address(std::array<uint8_t, 20> arr)
        : array(arr) {};
    Address(const AddressView v) { memcpy(data(), v.data(), size()); }
    using array::array;
    using array::data;
    using array::size;
    operator AddressView() const
    {
        return AddressView(data());
    }
    std::string to_string() const { return static_cast<AddressView>(*this).to_string(); }
    bool operator==(const AddressView other) const { return static_cast<AddressView>(*this) == other; }
    bool operator==(const Address other) const { return static_cast<AddressView>(*this) == other; }
    Address& operator=(const AddressView);
    std::array<uint8_t, 24> serialize() const { return static_cast<AddressView>(*this).serialize(); }
    struct Comparator {
        using is_transparent = std::true_type;
        static constexpr size_t offsetHash = 99;
        inline bool operator()(const Address& i1, const Address& i2) const
        {
            return i1 < i2;
        }
        inline bool operator()(const Address& i1, const AddressView i2) const
        {
            return static_cast<AddressView>(i1) < i2;
        }
        inline bool operator()(const AddressView i1, const Address& i2) const
        {
            return i1 < static_cast<AddressView>(i2);
        }
    };
};
