#pragma once
#include "transport/tcp/connection.hpp"
#include "conndata.hpp"
#include "spdlog/fmt/fmt.h"

inline std::string Conref::str() const
{
    return iter->second.c->to_string();
}
