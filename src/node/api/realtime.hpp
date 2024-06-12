#pragma once
#include "block/chain/height.hpp"
#include "block/chain/worksum.hpp"
#include "block/header/difficulty_declaration.hpp"
#include "nlohmann/json_fwd.hpp"

namespace mempool{
    struct Put;
    struct Erase;
}

class ConnectionBase;
namespace realtime_api{
    struct OnChainEvent {
        Height length;
        Target target;
        Worksum totalWork;
    };
    using nlohmann::json;
    void on_connect(size_t total, const ConnectionBase& c);
    void on_disconnect(size_t total, uint64_t id);
    void on_chain(OnChainEvent);
    void on_mempool_add(const mempool::Put&, size_t total);
    void on_mempool_erase(const mempool::Erase&, size_t total);
}
