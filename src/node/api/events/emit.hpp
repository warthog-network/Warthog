#include "block/chain/height.hpp"
#include "block/chain/worksum.hpp"
#include "block/header/difficulty_declaration.hpp"

namespace mempool {
struct Put;
struct Erase;
}
class ConnectionBase;

namespace api {
struct Block;
namespace event {
    struct OnChainEvent {
        Height length;
        Target target;
        Worksum totalWork;
    };
    void emit_connect(size_t total, const ConnectionBase& c);
    void emit_disconnect(size_t total, uint64_t id);
    void emit_chain_state(OnChainEvent);
    void emit_mempool_add(const mempool::Put&, size_t total);
    void emit_mempool_erase(const mempool::Erase&, size_t total);
    void emit_rollback(Height h);
    void emit_block_append(api::Block&&);
}
}
