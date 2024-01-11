#include "timestamprule.hpp"
#include "general/now.hpp"
#include <algorithm>

uint32_t TimestampValidator::get_valid_timestamp() const
{
    auto cpy { data };
    size_t index { (cpy.size() + 1) / 2 };
    std::nth_element(cpy.begin(), cpy.begin() + index, cpy.end());

    auto v = std::max(cpy[index], uint64_t(now_timestamp()));
    if (v + TOLERANCEMINUTES * 60 < tmax) {
        v = tmax - TOLERANCEMINUTES * 60;
    }
    return v;
}
