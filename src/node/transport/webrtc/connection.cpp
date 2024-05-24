#include "connection.hpp"
#include "eventloop/eventloop.hpp"
#include "webrtc_sockaddr.hpp"

// void RTCPendingOutgoing::set_answer(OneIpSdp sdp)
// {
//     peerIp = sdp.ip();
// }

void RTCConnection::async_send(std::unique_ptr<char[]> data, size_t size)
{
    assert(dc);
    std::string msg;
    msg.resize(size);
    std::copy(data.get(), data.get() + size, msg.begin());
    dc->send(std::move(msg));
}

std::shared_ptr<RTCConnection> RTCConnection::connect_new(std::weak_ptr<Eventloop> eventloop, sdp_callback_t cb, const IP& ip)
{
    spdlog::info("RTC connect_new with ip {}", ip.to_string());
    auto p { std::make_shared<RTCConnection>(false, std::move(eventloop), ip, OutPending()) };
    p->init(std::move(cb));
    p->set_data_channel(p->pc->createDataChannel("warthog"));
    return p;
}

std::shared_ptr<RTCConnection> RTCConnection::accept_new(std::weak_ptr<Eventloop> eventloop, sdp_callback_t cb, OneIpSdp sdp)
{
    spdlog::info("RTC connect_new with ip {}", sdp.ip().to_string());
    auto p { std::make_shared<RTCConnection>(true, std::move(eventloop), sdp.ip(), Default()) };
    p->init(std::move(cb));
    p->pc->onDataChannel([p = p.get()](std::shared_ptr<rtc::DataChannel> newdc) {
        if (p->dc)
            p->close(ERTCDUP_DATACHANNEL);
        else
            p->set_data_channel(std::move(newdc));
    });
    p->pc->setRemoteDescription({ sdp.sdp(), rtc::Description::Type::Offer });
    return p;
}

RTCConnection::RTCConnection(bool isInbound, std::weak_ptr<Eventloop> eventloop, IP ip, variant_t data)
    : isInbound(isInbound)
    , sockAddr { std::move(ip) }
    , eventloop(std::move(eventloop))
    , data(std::move(data))
{
}

void RTCConnection::set_data_channel(std::shared_ptr<rtc::DataChannel> newdc)
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
        pc->close();
    });
    dc->onError([&](std::string error) { 
            close(ERTCCHANNEL);
            std::cout << "ERROR: " << error << std::endl; });
}

void RTCConnection::init(sdp_callback_t sdpCallback)
{
    rtc::Configuration config {
        .iceServers {
            // { "stun:stun.l.google.com:19302" }
        }
    };
    pc = std::make_shared<rtc::PeerConnection>(config);
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

void RTCConnection::close(int errcode)
{
    set_error(errcode);
    pc->close();
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
    pc->setRemoteDescription(sdp.sdp());
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
