#pragma once
#include <variant>

namespace token_data {
struct ForkData {
};

class TokenData {
    using variant_t = std::variant<ForkData>;

    ForkData* get_fork_data()
    {
        if (std::holds_alternative<ForkData>(data)) {
            return &std::get<ForkData>(data);
        }
        return nullptr;
    }

    auto visit(auto lambda) const
    {
        return std::visit(lambda, data);
    }

private:
    variant_t data;
};
}
