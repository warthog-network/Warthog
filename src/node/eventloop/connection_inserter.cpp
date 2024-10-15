#include "connection_inserter.hpp"
#include "eventloop/eventloop.hpp"

void ConnectionInserter::evict(Conref cr) const{
    eventloop.close(cr, EEVICTED);
}
ConState ConnectionInserter::make_connection_state(std::shared_ptr<ConnectionBase> p) const {
    return {std::move(p),eventloop};
}
