#include "rtc_connection.hxx"
#include "eventloop/eventloop.hpp"
#include "webrtc_sockaddr.hpp"

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
    std::vector<std::byte> msg;
    msg.resize(size);
    memcpy(msg.data(), data.get(), size);
#ifdef DISABLE_LIBUV
    proxy_to_main_runtime([p = shared_from_this(), msg = std::move(msg)]() mutable {
        p->send_proxied(std::move(msg));
    });
#else
    send_proxied(std::move(msg));
#endif
}

std::shared_ptr<RTCConnection> RTCConnection::connect_new(Eventloop& eventloop, sdp_callback_t cb, const IP& ip, uint64_t verificationConId)
{
    // TODO: count active rtc connections
    spdlog::info("RTC connect_new with ip {}", ip.to_string());
    auto p { std::make_shared<RTCConnection>(false, verificationConId, eventloop.weak_from_this(), ip, OutPending()) };
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

std::shared_ptr<RTCConnection> RTCConnection::accept_new(Eventloop& eventloop, sdp_callback_t cb, OneIpSdp sdp, uint64_t verificationConId)
{
    spdlog::info("RTC accept_new with ip {}", sdp.ip().to_string());
    auto p { std::make_shared<RTCConnection>(true, verificationConId, eventloop.weak_from_this(), sdp.ip(), Default()) };
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

RTCConnection::RTCConnection(bool isInbound, uint64_t verificationConId, std::weak_ptr<Eventloop> eventloop, IP ip, variant_t data)
    : isInbound(isInbound)
    , verificationConId(verificationConId)
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

    dc->onOpen([&, opened = false]() mutable {
        if (opened == false) { // for some reason in firefox browsers
                               // this is called multiple times
                               // so we need to make sure this only acts once.
            opened = true;
            std::cout << "Datachannel OPEN " << this << std::endl;
            on_connected();
        }
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

void RTCConnection::close(Error e)
{
    if (!set_error(e))
        return;
    spdlog::info("Close rtc connection {}", e.strerror());
#ifdef DISABLE_LIBUV
    proxy_to_main_runtime([p = shared_from_this()]() {
        p->pc.reset(); // triggers destructor
    });
#else
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
    std::lock_guard l(errcodeMutex);
    if (!error.has_value()) {
        error = Error(ERTCCLOSED);
    }
    on_close({
        .error = *error,
    });
    if (auto e { eventloop.lock() })
        e->notify_closed_rtc(shared_from_this());
}

bool RTCConnection::set_error(Error e)
{
    std::lock_guard l(errcodeMutex);
    if (!error.has_value()) {
        error = e; // TODO: on_error(errcode) in destructor
        return true;
    }
    return false;
}

//////////////////////////////
/// maybe proxied functions (for emscripten build wasm-datachannel functions need to be called from main browser thread due because they access javascript "Window" object.)

void RTCConnection::init_proxied(sdp_callback_t&& sdpCallback)
{
    spdlog::info("init proxied");
    rtc::Configuration config;
    config.iceServers.push_back({ "stun:stun.l.google.com:19302" });

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

void RTCConnection::send_proxied(std::vector<std::byte>&& msg)
{
    {
        std::lock_guard l(errcodeMutex);
        if (error.has_value())
            return;
    }
    assert(dc);
    dc->send(std::move(msg));
}
