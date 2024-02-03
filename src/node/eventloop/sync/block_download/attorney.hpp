#pragma once
#include "attorney_declaration.hpp"
#include "block/chain/header_chain.hpp"
#include "eventloop/eventloop.hpp"
class Eventloop;
namespace BlockDownload {
inline auto Attorney::connections() const
{
    return eventloop.connections.initialized();
}

inline auto Attorney::set_stage_headers(Headerchain&& newheaders)
{
    return eventloop.set_stage_headers(std::move(newheaders));
}
}
