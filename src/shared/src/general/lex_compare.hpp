#pragma once
#include <utility>

template <typename Projection, typename... Projections>
bool lex_less_project(const auto& lhs, const auto& rhs, Projection&& projection, Projections&&... projections)
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
        return lex_less_project(lhs, rhs, std::forward<Projections>(projections)...);
    }
}

namespace mempool{
namespace extractors {
template <typename T>
struct GetExtractor;
}
}
template <typename... ByTypes>
bool lex_less_by(const auto& lhs, const auto& rhs)
{
    return lex_less_project(lhs, rhs, mempool::extractors::GetExtractor<ByTypes>::value...);
}
