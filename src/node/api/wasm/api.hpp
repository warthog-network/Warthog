#pragma once
#include "nlohmann/json_fwd.hpp"


namespace api{
    using nlohmann::json;
    void emit_connect_count(size_t N);
    void emit_connect(json);
    void emit_disconnect(json);
    void emit_chain(json);
    void emit_mempool_add(json);
    void emit_mempool_erase(json);
    void emit_api_result(size_t id, std::string s);
}
