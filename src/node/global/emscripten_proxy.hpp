#pragma once
#include <emscripten/proxying.h>
#include <emscripten/threading.h>

extern emscripten::ProxyingQueue globalProxyingQueue;
inline void proxy_to_main_runtime(auto cb)
{
    globalProxyingQueue.proxyAsync(emscripten_main_runtime_thread_id(), std::move(cb));
}
