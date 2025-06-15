#pragma once
#include "../uint64/types.hpp"
#include "general/funds.hpp"
namespace defi {
struct BaseQuote {
    BaseQuote(BaseQuote_uint64 bq)
        : base(bq.base)
        , quote(Wart::from_funds_throw(bq.quote)) { };
    Funds_uint64 base;
    Wart quote;
};

}
