#pragma once
#include "defi/token/info.hpp"
namespace api {
struct AssetLookupTrace { // for debugging
    std::vector<AssetDetail> fails;
    wrt::optional<Height> snapshotHeight;
};
}
