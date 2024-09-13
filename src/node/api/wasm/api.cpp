#include "api.hpp"
#include "global/emscripten_proxy.hpp"
#include "nlohmann/json.hpp"

extern "C" {
extern void onConnectCount(size_t N);
extern void onConnect(const char*);
extern void onDisconnect(const char*);
extern void onChain(const char*);
extern void onMempoolAdd(const char*);
extern void onMempoolErase(const char*);
extern void onAPIResult(size_t, const char*);
}

namespace api {
void emit_connect_count(size_t N)
{
    proxy_to_main_runtime([N]() {
        onConnectCount(N);
    });
}

template <void (*fun)(const char*)>
void proxy_json_call(const json& j)
{
    proxy_to_main_runtime([s = j.dump(0)]() { fun(s.c_str()); });
}

void emit_api_result(size_t id, std::string s)
{
    proxy_to_main_runtime([id, s = std::move(s)]() { onAPIResult(id, s.c_str()); });
}

void emit_connect(json j)
{
    proxy_json_call<onConnect>(j);
}

void emit_disconnect(json j)
{
    proxy_json_call<onDisconnect>(j);
}
void emit_chain(json j)
{
    proxy_json_call<onChain>(j);
}
void emit_mempool_add(json j)
{
    proxy_json_call<onMempoolAdd>(j);
}
void emit_mempool_erase(json j)
{
    proxy_json_call<onMempoolErase>(j);
}
}
