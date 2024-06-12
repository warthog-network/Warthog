#pragma once
#include "nlohmann/json_fwd.hpp"


namespace wasm_api{
    using nlohmann::json;
    void on_connect_count(size_t N);
    void on_connect(json);
    void on_disconnect(json);
    void on_chain(json);
    void on_mempool_add(json);
    void on_mempool_erase(json);
}
