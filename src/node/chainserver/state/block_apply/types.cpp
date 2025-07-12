#include "types.hpp"

Hash RewardInternal::hash() const
{
    return HasherSHA256()
        << toAddress
        << wart
        << height
        << uint16_t(0);
}
