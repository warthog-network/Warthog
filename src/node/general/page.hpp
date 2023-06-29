#pragma once
#include "general/errors.hpp"
#include <cassert>
#include <cstdint>

struct Page {
    Page(uint32_t page)
        : p(page)
    {
        if (p == 0)
            throw Error(EMALFORMED);
    };
    uint32_t val() { return p; }

private:
    const uint32_t p;
};
