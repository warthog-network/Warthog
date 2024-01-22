#include "log_compressed.hpp"
#include <cstdint>
#include "general/errors.hpp"
#include "block/body/parse.hpp"
#include "block/body/primitives.hpp"
#include "spdlog/spdlog.h"
// experimental functionality to further decrease byte size of TransferTxExchangeMessages to save bandwidth

    uint64_t c0(uint64_t x)
    {
        uint64_t y = x;
        y += 0xfffffffffffff81a;
        y *= x;
        y += 0x0000000000147e7b;
        y *= x;
        y += 0xffffffffee99449a;
        return y;
    }

    uint64_t c1(uint64_t x)
    {
        uint64_t y = x;
        y += 0xfffffffffffff89a;
        y *= x;
        y += 0x0000000000122f09;
        y *= x;
        y += 0xfffffffff128aef0;
        return y;
    }
    void assert_no_abort(bool condition)
    {
        if (!condition) {
            throw Error(EBUG);
        }
    }

    void log_compressed(const TransferView& t)
    {
        // log compression of Transfer items, this may be implemented in future to further decrease byte size of items
        auto c_0 { c0(t.fromAccountId().value()) };
        auto c_1 { c1(t.fromAccountId().value()) };
        assert_no_abort(c_0 != 0);
        assert_no_abort(c_1 != 0);

        // extract upper part of c's
        uint32_t c_upper = (c_1 >> 32) ^ (c_0 >> 32);

        // recovery bytes
        // (we can omit one and retrieve it later with trial/error looping over 65536 possiblities, which is feasible)
        // This will save 2 bytes (sizeof(c_upper) + sizeof(c_0_r) = 6 vs 8 = sizeof(accountId))
        uint16_t c_0_r = (c_0 & 0xFFFF);
        uint16_t c_1_r = (c_1 & 0xFFFF);

        // TODO: the omitted would then be xor'ed into compactFee, (note that fee cannot be larger than balance, so this gives condition to get back valid values of omitted)

        spdlog::debug("Compression of accountId: c_upper = {}, c_0_r = {}, c_1_r = {}", c_upper, c_0_r, c_1_r);
    }

    void log_compressed(const TransferTxExchangeMessage& t)
    {
        // log compression of Transfer items, this may be implemented in future to further decrease byte size of items
        auto c_0 { c0(t.from_id().value()) };
        auto c_1 { c1(t.from_id().value()) };
        assert_no_abort(c_0 != 0);
        assert_no_abort(c_1 != 0);

        // extract upper part of c's
        uint32_t c_upper = (c_1 >> 32) ^ (c_0 >> 32);

        // recovery bytes
        // (we can omit one and retrieve it later with trial/error looping over 65536 possiblities, which is feasible)
        // This will save 2 bytes (sizeof(c_upper) + sizeof(c_0_r) = 6 vs 8 = sizeof(accountId))
        uint16_t c_0_r = (c_0 & 0xFFFF);
        uint16_t c_1_r = (c_1 & 0xFFFF);

        // TODO: the omitted would then be xor'ed into compactFee, (note that fee cannot be larger than balance, so this gives condition to get back valid values of omitted)

        spdlog::debug("Compression of accountId: c_upper = {}, c_0_r = {}, c_1_r = {}", c_upper, c_0_r, c_1_r);
    }
