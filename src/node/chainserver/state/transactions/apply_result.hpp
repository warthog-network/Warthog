#pragma once
#include "general/errors_forward.hpp"
namespace chainserver {
struct ApplyResult : public ChainError {
    using ChainError::ChainError;
};
}
