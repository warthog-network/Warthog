#pragma once
#include "connection.hpp"
#include "rtc/rtc.hpp"
#ifdef DISABLE_LIBUV
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#endif
#ifdef DISABLE_LIBUV
emscripten::ProxyingQueue& proxying_queue();
inline void proxy_to_main_runtime(auto cb)
{
    proxying_queue().proxyAsync(emscripten_main_runtime_thread_id(), std::move(cb));
}
#endif

template <typename callback_t>
requires std::is_invocable_v<callback_t, IdentityIps&&>
void RTCConnection::fetch_id(callback_t cb, bool stun)
{
#ifdef DISABLE_LIBUV
    proxy_to_main_runtime([cb = std::move(cb), stun]() {
#endif
        rtc::Configuration cfg;
        if (stun)
            cfg.iceServers.push_back({ "stun:stun.l.google.com:19302" });

        auto pc = std::make_shared<rtc::PeerConnection>(cfg);
        pc->onGatheringStateChange(
            [pc, on_result = std::move(cb)](rtc::PeerConnection::GatheringState state) mutable {
                if (state == rtc::PeerConnection::GatheringState::Complete) {
                    std::string sdp(pc->localDescription().value());
                    on_result(IdentityIps::from_sdp(sdp));
                    pc.reset();
                }
            });
        auto dc { pc->createDataChannel("") };
#ifdef DISABLE_LIBUV
    });
#endif
}
