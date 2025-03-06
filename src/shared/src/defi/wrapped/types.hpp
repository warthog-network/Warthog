#pragma once
#include "../uint64/types.hpp"
#include "general/funds.hpp"
namespace defi {
class BaseQuote {

    BaseQuote(BaseQuote_uint64 bq)
        : data(bq) { };
    Funds base() const
    {
        return Funds::from_value_throw(data.base);
        uint64_t base;
        uint64_t quote;
    }

private:
    BaseQuote_uint64 data;
}

}
