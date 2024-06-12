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
}

namespace wasm_api {
void on_connect_count(size_t N)
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

void on_connect(json j)
{
    proxy_json_call<onConnect>(j);
}

void on_disconnect(json j)
{
    proxy_json_call<onDisconnect>(j);
}
void on_chain(json j)
{
    proxy_json_call<onChain>(j);
}
void on_mempool_add(json j)
{
    proxy_json_call<onMempoolAdd>(j);
}
void on_mempool_erase(json j)
{
    proxy_json_call<onMempoolErase>(j);
}
}
