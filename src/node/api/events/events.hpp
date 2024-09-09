#pragma once
#include "../types/all.hpp"
namespace api {

namespace event {
    using Event = std::variant<Rollback, Block>;
}
using Event = event::Event;
}

