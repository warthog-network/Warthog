#pragma once
#include <utility>

template <typename T, typename Projection, typename... Projections>
bool lex_compare_less_by(const T& lhs, const T& rhs, Projection&& projection, Projections&&... projections)
{
    auto lhs_val { projection(lhs) };
    auto rhs_val { projection(rhs) };
    if (lhs_val < rhs_val)
        return true;
    if (rhs_val < lhs_val)
        return false;
    if constexpr (sizeof...(projections) == 0) {
        return false;
    } else {
        return lex_compare_les(lhs, rhs, std::forward<Projections>(projections)...);
    }
}
