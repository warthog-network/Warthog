#pragma once
#include "block/chain/height.hpp"
#include "general/errors.hpp"
namespace chainserver {
struct ApplyResult:public ChainError {
    using ChainError::ChainError;
};
}
