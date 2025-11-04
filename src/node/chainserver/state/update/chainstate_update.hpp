#pragma once

#include "block/chain/header_chain.hpp"
#include <variant>

namespace chainserver {
namespace state_update {
    struct Fork : public HeaderchainFork {
        std::shared_ptr<Headerchain> prevChain;
        wrt::optional<SignedSnapshot> signedSnapshot;
    };

    struct SignedSnapshotApply {
        struct Rollback {
            HeaderchainRollback deltaHeaders;
            std::shared_ptr<Headerchain> prevHeaders;
        };
        wrt::optional<Rollback> rollback;
        SignedSnapshot signedSnapshot;
    };

    struct Append {
        HeaderchainAppend headerchainAppend;
        wrt::optional<SignedSnapshot> signedSnapshot;
    };

    using variant_t = std::variant<
        Fork,
        Append,
        SignedSnapshotApply>;

    struct ChainstateUpdate : public variant_t {
        using variant_t::variant;
        wrt::optional<ShrinkInfo> rollback() const {
            if (std::holds_alternative<Fork>(*this)) {
                return std::get<Fork>(*this).shrink;
            }else if (std::holds_alternative<SignedSnapshotApply>(*this)){
                auto rb{std::get<SignedSnapshotApply>(*this).rollback};
                if (rb) {
                    return rb->deltaHeaders.shrink;
                }
            }
            return wrt::nullopt;
        }
    };
}
}
