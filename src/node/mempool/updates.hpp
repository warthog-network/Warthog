#pragma once
#include "entry.hpp"
#include <variant>


namespace mempool{

struct Put {
     Entry entry;
};
struct Erase {
    TransactionId id;
};
using Update = std::variant<Put, Erase>;
using Updates = std::vector<Update>;
}
