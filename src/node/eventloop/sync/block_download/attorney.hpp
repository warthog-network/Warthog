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

inline auto Attorney::update_blockdownlad(Headerchain&& newheaders)
{
    return eventloop.chains.update_stage(std::move(newheaders));
}
}
