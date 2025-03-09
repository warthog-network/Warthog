#pragma once
#include "conndata.hpp"
#include "global/globals.hpp"
#include "spdlog/fmt/fmt.h"

inline std::string Conref::str() const
{
    return iter->second.c->tag_string();
}
template <typename T>
void Conref::send(T&& t)
{
    communication_log().info("OUT {}: {}", str(), t.log_str());
    send_buffer(std::forward<T>(t)); // implicitly convert to Sndbuffer
}
