#include "mine.hpp"
#include "block/header/difficulty.hpp"
#include "block/header/view.hpp"
#include "block/header/header_impl.hpp"
#include "spdlog/spdlog.h"
#include "third_party/cpuminer-opt/hash.hpp"
#include <cstring>

inline Hash hash_cpuminer(HeaderView hv)
{
    Hash h;
    sha256d(h.data(), hv.data(), hv.size());
    return h;
}

std::tuple<bool, bool, uint32_t> mine(Header& header, uint32_t stop, uint32_t tries)
{
    const TargetV1 t = header.target_v1();
    const uint32_t nonce { header.nonce() };
    uint32_t end = nonce + std::min(stop - nonce, tries);

    uint32_t i = nonce;
    while (true) {
        // check hash
        if (t.compatible(hash_cpuminer(header))) {
            return { true, i + 1 == stop, i + 1 - nonce };
        }
        header.set_nonce(++i);
        if (i == end)
            break;
    }

    return { false, end == stop, end - nonce };
}
