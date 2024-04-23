#pragma once
#include "connection_schedule.hpp"
#include <random>
#include <algorithm>
template <typename addr_t>
std::vector<addr_t> connection_schedule::VerifiedVector<addr_t>::sample(size_t N) const
{
    std::vector<addr_t> out;
    out.reserve(N);
    std::sample(this->data.begin(), this->data.end(), std::back_inserter(out),
        N, std::mt19937 { std::random_device {}() });
    return out;
};
