#pragma once

#include "block/chain/header_chain.hpp"
#include <variant>

namespace chainserver {
namespace state_update {
    struct Fork : public HeaderchainFork {
        std::shared_ptr<Headerchain> prevChain;
        std::optional<SignedSnapshot> signedSnapshot;
    };

    struct RollbackData {
        struct Data {
            HeaderchainRollback rollback;
            std::shared_ptr<Headerchain> prevChain;
        };
        std::optional<Data> data;
        SignedSnapshot signedSnapshot;
    };

    struct Append {
        HeaderchainAppend headerchainAppend;
        std::optional<SignedSnapshot> signedSnapshot;
    };

    using ChainstateUpdate = std::variant<
        Fork,
        Append,
        RollbackData>;
}
}
