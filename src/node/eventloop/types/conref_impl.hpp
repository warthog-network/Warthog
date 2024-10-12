#pragma once
#include "conndata.hpp"
#include "spdlog/fmt/fmt.h"

inline std::string Conref::str() const
{
    return iter->second.c->tag_string();
}
