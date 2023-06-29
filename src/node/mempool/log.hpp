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
using Action = std::variant<Put, Erase>;
using Log = std::vector<Action>;
}
