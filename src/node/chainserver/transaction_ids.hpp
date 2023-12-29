#pragma once
#include "block/body/transaction_id.hpp"
#include <set>
namespace chainserver {

struct ByPinHeight {
    bool operator()(const TransactionId& tid1, const TransactionId& tid2) const
    {
        if (tid1.pinHeight == tid2.pinHeight)
            return tid1 < tid2;
        return tid1.pinHeight < tid2.pinHeight;
    }
};
struct TransactionIds : public std::set<TransactionId, ByPinHeight> {
    static std::pair<Height, Height> block_range(Height length)
    {
        Height upper { length + 1 }; // height of next block
        Height lower = upper.pin_begin() + 1; // +1 because at pin_begin we cannot have
                                              // the same pinHeight
        return { lower, upper };
    }
    void prune(Height length)
    {
        const Height minPinHeight { (length + 1).pin_begin() };
        auto iter = begin();
        while (iter != end() && iter->pinHeight < minPinHeight)
            erase(iter++);
    }
};
}
