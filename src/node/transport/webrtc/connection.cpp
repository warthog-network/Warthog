#include "connection.hpp"
#include "eventloop/eventloop.hpp"
#include "webrtc_sockaddr.hpp"
#ifdef DISABLE_LIBUV
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
namespace {
auto& proxying_queue()
{
    static emscripten::ProxyingQueue q;
    return q;
}
void proxy_to_main_runtime(auto cb)
{
    proxying_queue().proxyAsync(emscripten_main_runtime_thread_id(), std::move(cb));
}
}
#endif

void RTCConnection::if_not_closed(auto lambda)
{
    std::lock_guard l(errcodeMutex);
    lambda();
}

// void RTCPendingOutgoing::set_answer(OneIpSdp sdp)
// {
//     peerIp = sdp.ip();
// }

void RTCConnection::async_send(std::unique_ptr<char[]> data, size_t size)
{
    std::string msg;
    msg.resize(size);
    std::copy(data.get(), data.get() + size, msg.begin());
#ifdef DISABLE_LIBUV
    proxy_to_main_runtime([p = shared_from_this(), msg = std::move(msg)]() {
        p->send_proxied(std::move(msg));
    });
#else
    send_proxied(std::move(msg));
#endif
}

std::shared_ptr<RTCConnection> RTCConnection::connect_new(std::weak_ptr<Eventloop> eventloop, sdp_callback_t cb, const IP& ip)
{
    spdlog::info("RTC connect_new with ip {}", ip.to_string());
    auto p { std::make_shared<RTCConnection>(false, std::move(eventloop), ip, OutPending()) };
#ifdef DISABLE_LIBUV
    proxy_to_main_runtime(
        [p, cb = std::move(cb)]() mutable {
            p->init_proxied(std::move(cb));
            p->set_data_channel_proxied(p->pc->createDataChannel("warthog"));
        });
#else
    p->init_proxied(std::move(cb));
    p->set_data_channel_proxied(p->pc->createDataChannel("warthog"));
#endif
    return p;
}

std::shared_ptr<RTCConnection> RTCConnection::accept_new(std::weak_ptr<Eventloop> eventloop, sdp_callback_t cb, OneIpSdp sdp)
{
    spdlog::info("RTC connect_new with ip {}", sdp.ip().to_string());
    auto p { std::make_shared<RTCConnection>(true, std::move(eventloop), sdp.ip(), Default()) };
#ifdef DISABLE_LIBUV
    proxy_to_main_runtime(
        [p, cb = std::move(cb), sdp = std::move(sdp)]() mutable {
            p->init_proxied(std::move(cb));
            p->pc->onDataChannel([p = p.get()](std::shared_ptr<rtc::DataChannel> newdc) {
                if (p->dc)
                    p->close(ERTCDUP_DATACHANNEL);
                else
                    p->set_data_channel_proxied(std::move(newdc));
            });
            p->pc->setRemoteDescription({ sdp.sdp(), rtc::Description::Type::Offer });
        });
#else
    p->init_proxied(std::move(cb));
    p->pc->onDataChannel([p = p.get()](std::shared_ptr<rtc::DataChannel> newdc) {
        if (p->dc)
            p->close(ERTCDUP_DATACHANNEL);
        else
            p->set_data_channel_proxied(std::move(newdc));
    });
    p->pc->setRemoteDescription({ sdp.sdp(), rtc::Description::Type::Offer });
#endif
    return p;
}

RTCConnection::RTCConnection(bool isInbound, std::weak_ptr<Eventloop> eventloop, IP ip, variant_t data)
    : isInbound(isInbound)
    , sockAddr { std::move(ip) }
    , eventloop(std::move(eventloop))
    , data(std::move(data))
{
}

void RTCConnection::set_data_channel_proxied(std::shared_ptr<rtc::DataChannel> newdc)
{
    assert(!dc);
    dc = std::move(newdc);
    std::cout << "New data channel" << std::endl;

    dc->onOpen([&]() {
        std::cout << "Datachannel OPEN" << std::endl;
        on_connected();
    });
    dc->onMessage([&](rtc::message_variant msg) {
        if (closed())
            return;

        if (std::holds_alternative<std::string>(msg)) {
            close(ERTCTEXT);
            return;
        }
        auto& msgdata { std::get<rtc::binary>(msg) };
        std::span<uint8_t> s(reinterpret_cast<uint8_t*>(msgdata.data()), msgdata.size());
        on_message(s);
    });
    dc->onClosed([&]() {
        std::cout << "CLOSED" << std::endl;
        close(ERTCCHANNEL_CLOSED);
    });
    dc->onError([&](std::string error) { 
        close(ERTCCHANNEL_ERROR);
        std::cout << "ERROR: " << error << std::endl; });
}

void RTCConnection::close(int errcode)
{
    set_error(errcode);
#ifdef DISABLE_LIBUV
    proxy_to_main_runtime([p = shared_from_this()]() {
        p->pc.reset(); // triggers destructor
    });
#else
    pc->close();
    pc.reset(); // triggers destructor
#endif
}

// called by eventloop thread
std::optional<Error> RTCConnection::set_sdp_answer(OneIpSdp sdp)
{
    // TODO update port;
    spdlog::info("Setting remote description for ip {}", sdp.ip().to_string());
    assert(std::holds_alternative<OutPending>(data));
    if (sdp.ip() != sockAddr.ip) {
        return Error(ERTCIP_FA);
    }
    data = Default {};
#ifdef DISABLE_LIBUV
    proxy_to_main_runtime(
        [p = shared_from_this(), sdp = std::move(sdp)]() mutable {
            p->if_not_closed([&p, &sdp]() {
                p->pc->setRemoteDescription({ sdp.sdp(), rtc::Description::Type::Answer });
            });
        });
#else
    if_not_closed([this, &sdp]() {
        pc->setRemoteDescription({ sdp.sdp(), rtc::Description::Type::Answer });
    });
#endif
    return {};
}

void RTCConnection::notify_closed()
{
    set_error(ERTCCLOSED);
    on_close({
        .error = errcode,
    });
}

void RTCConnection::set_error(int error)
{
    std::lock_guard l(errcodeMutex);
    if (errcode == 0)
        errcode = error; // TODO: on_error(errcode) in destructor
}

//////////////////////////////
/// maybe proxied functions (for emscripten build wasm-datachannel functions need to be called from main browser thread due because they access javascript "Window" object.

void RTCConnection::init_proxied(sdp_callback_t&& sdpCallback)
{
    rtc::Configuration config {
        .iceServers {
            // { "stun:stun.l.google.com:19302" }
        }
    };

    std::lock_guard l { errcodeMutex };
    pc = std::make_unique<rtc::PeerConnection>(config);
    pc->onStateChange([p = shared_from_this()](rtc::PeerConnection::State state) mutable {
        if (p) {
            using enum rtc::PeerConnection::State;
            if (state == Failed) {
                auto e { p->eventloop.lock() };
                p->close(ERTCFAILED);
            }
            if (state == Closed) {
                auto e { p->eventloop.lock() };
                p->notify_closed();
                p.reset(); // release connection
            }
        }
    });

    ; // connection id of signaling server
    pc->onGatheringStateChange(
        [this, sdpCallback = std::move(sdpCallback)](rtc::PeerConnection::GatheringState state) {
            if (state == rtc::PeerConnection::GatheringState::Complete) {
                auto description = pc->localDescription();

                sdpCallback(*this, std::string(description.value()));
            }
        });
}

void RTCConnection::send_proxied(std::string msg)
{
    {
        std::lock_guard l(errcodeMutex);
        if (errcode)
            return;
    }
    assert(dc);
    dc->send(std::move(msg));
}
