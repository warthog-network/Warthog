#pragma once

#include "block/chain/height.hpp"
#include <optional>
#include <variant>
class POWVersion {
    struct NonV2_2 {
        static constexpr bool verusv2_2 = false;
    };
    struct V2_2 {
        static constexpr bool verusv2_2 = true;
    };

public:
    struct Original : public NonV2_2 { };
    struct Janus1 : public NonV2_2 { };
    struct Janus2 : public NonV2_2 { };
    struct Janus3 : public NonV2_2 { };
    struct Janus4 : public NonV2_2 { };
    struct Janus5 : public NonV2_2 { };
    struct Janus6 : public NonV2_2 { };
    struct Janus7 : public NonV2_2 { };
    struct Janus8 : public V2_2 { };
    [[nodiscard]] static std::optional<POWVersion> from_params(
        NonzeroHeight height, uint32_t version, bool testnet);
    auto visit(const auto& lambda) const
    {
        return std::visit(lambda, data);
    }
    [[nodiscard]] bool uses_verus_2_2() const
    {
        return visit([](auto& e) { return e.verusv2_2; });
    }

private:
    using variant_t = std::variant<Original, Janus1, Janus2, Janus3, Janus4, Janus5, Janus6, Janus7, Janus8>;
    POWVersion(variant_t data)
        : data(data)
    {
    }

    variant_t data;
};
