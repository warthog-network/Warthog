#pragma once
#include "defi/token/info.hpp"
namespace api {
struct AssetLookupTrace { // for debugging
    std::vector<AssetDetail> fails;
    std::optional<Height> snapshotHeight;
};
}
