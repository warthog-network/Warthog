#include "orderbook.hpp"
#include "pool.hpp"

namespace defi {
namespace {
struct PreparedExtradata {
    struct ExtraData {
        uint64_t cumsum;
        size_t upperBoundCounterpart;
    };
    PreparedExtradata(const Orderbook_uint64& b)
    {
        extraBase.resize(0);
        extraQuote.resize(0);
        uint64_t cumsumQuote { 0 };
        const size_t J { b.base_asc_sell().size() };
        const size_t I { b.quote_desc_buy().size() };
        uint64_t cumsumBase { b.base_asc_sell().total_push().value() };
        size_t j { 0 };
        extraBase.resize(0);
        for (size_t i = 0; i < I; ++i) {
            auto& oq { b.quote_desc_buy()[i] };
            for (; j < J; ++j) {
                auto& ob { b.base_asc_sell()[J - 1 - j] };
                if (ob.limit <= oq.limit)
                    break;
                extraBase.push_back({ cumsumBase, i });
                cumsumBase -= ob.amount.value();
            }
            extraQuote.push_back({ cumsumQuote, j });
            cumsumQuote += oq.amount.value();
        }
        for (; j < J; ++j) {
            extraBase.push_back({ cumsumBase, I });
            cumsumBase -= b.base_asc_sell()[J - 1 - j].amount.value();
        }
    }
    std::vector<ExtraData> extraQuote;
    std::vector<ExtraData> extraBase;
};
}
auto Orderbook_uint64::match(const PoolLiquidity_uint64& p) const
    -> MatchResult_uint64
{
    // using namespace std;
    // auto print_vec {
    //     [](auto& vec) {
    //         for (size_t i { 0 }; i < vec.size(); ++i) {
    //             cout << "Limit: " << vec[i].limit.to_double() << " amount: " << vec[i].amount.value() << endl;
    //         }
    //     }
    // };

    // cout << "Base sell" << endl;
    // print_vec(base_asc_sell());
    // cout << "Quote buy" << endl;
    // print_vec(quote_desc_buy());
    PreparedExtradata prepared { *this };
    const auto& extraQuote { prepared.extraQuote };
    const auto& extraBase { prepared.extraBase };
    // for (auto &e : extraQuote) {
    //     cout<< "cumsum: "<<e.cumsum<<" K="<<e.upperBoundCounterpart<<endl;
    // }
    // for (auto &e : extraBase) {
    //     cout<< "cumsum: "<<e.cumsum<<" K="<<e.upperBoundCounterpart<<endl;
    // }
    const size_t I { pushQuoteDesc.size() };
    const size_t J { pushBaseAsc.size() };
    size_t i0 { 0 };
    size_t i1 { I };

    Matcher m { p };

    while (i0 != i1) {
        auto i { (i0 + i1) / 2 };
        auto& eq { extraQuote[i] };
        auto j { eq.upperBoundCounterpart };
        m.in.base = (j == J ? 0 : extraBase[j].cumsum);
        m.in.quote = eq.cumsum;
        if (m.bisection_step(pushQuoteDesc[i].limit))
            i0 = i + 1;
        else
            i1 = i;
    }
    auto bisect_j = [&](size_t j0, size_t j1) -> MatchResult_uint64 {
        while (j0 != j1) {
            auto j { (j0 + j1) / 2 };
            m.in.base = extraBase[j].cumsum;
            if (m.bisection_step(pushBaseAsc[J - 1 - j].limit))
                j0 = j + 1;
            else
                j1 = j;
        }
        if (j1 == 0) {
            return m.bisect_dynamic_price();
        } else {
            auto j { j1 - 1 };
            m.in.base = extraBase[j].cumsum - pushBaseAsc[J - 1 - j].amount.value();
            auto price { pushBaseAsc[J - 1 - j].limit };
            if (m.bisection_step(price)) {
                return m.bisect_dynamic_price();
            } else {
                return m.bisect_fixed_price(false, extraBase[j].cumsum, m.in.base, price);
            }
        }
    };
    if (i1 == 0) {
        size_t j0 = 0;
        size_t j1 = (I == 0 ? J : extraQuote[0].upperBoundCounterpart);
        m.in.quote = 0;
        return bisect_j(j0, j1);
    } else {
        auto i { i1 - 1 };
        auto& eq { extraQuote[i] };
        auto price { pushQuoteDesc[i].limit };
        auto j { eq.upperBoundCounterpart };
        m.in.base = (j == J ? 0 : extraBase[j].cumsum);
        m.in.quote = eq.cumsum + pushQuoteDesc[i].amount.value();
        size_t j0 = extraQuote[i].upperBoundCounterpart;
        if (m.bisection_step(price)) {
            size_t j1 = (i1 < I ? extraQuote[i1].upperBoundCounterpart : J);
            return bisect_j(j0, j1);
        } else {
            return m.bisect_fixed_price(true, eq.cumsum, m.in.quote, price);
        }
    }
}
} // namespace defi
