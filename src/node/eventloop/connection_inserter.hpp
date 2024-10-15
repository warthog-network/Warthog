#pragma once

#include "eventloop/types/conref_declaration.hpp"
#include <memory>
class Eventloop;
class ConnectionInserter {
    friend class Eventloop;

public:
    ConnectionInserter(Eventloop& el)
        : eventloop(el)
    {
    }
    ConState make_connection_state(std::shared_ptr<ConnectionBase> p) const;
    void evict(Conref cr) const;

private:
    Eventloop& eventloop;
};
