#pragma once
#include "asyncio/connection.hpp"
#include "conndata.hpp"
#include "spdlog/fmt/fmt.h"

inline std::string Conref::str() const
{
    return static_cast<const Connection*>(*this)->to_string();
}
