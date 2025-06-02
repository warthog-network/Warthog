#pragma once

#include "variant.hpp"
#include <optional>
namespace wrt { // wart tools namesapce
template <typename Id, typename Alt>
struct alternative {
public:
    alternative(Id id)
        : v(id)
    {
    }
    alternative(Alt a)
        : v(std::move(a))
    {
    }

    auto visit(auto lambda)
    {
        return v.visit(lambda);
    }
    auto visit(auto lambda) const
    {
        return v.visit(lambda);
    }
    std::optional<Id> map_alternative(auto lambda) const
    {
        return v.visit_overload(
            [&](Id id) -> std::optional<Id> { return id; },
            [&](const Alt& a) -> std::optional<Id> { return lambda(a); });
    }

private:
    wrt::variant<Id, Alt> v;
};
}
