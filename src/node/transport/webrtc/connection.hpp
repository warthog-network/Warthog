#pragma once
#include "../connection_base.hpp"
#include "communication/buffers/recvbuffer.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "eventloop/types/conref_declaration.hpp"
#include <memory>

class Eventloop;

namespace rtc {
class DataChannel;
class PeerConnection;
}

class RTCConnection final : public ConnectionBase, public std::enable_shared_from_this<RTCConnection> {

    struct OutPending {
    };

    struct Default {
    };
    using variant_t = std::variant<OutPending, Default>;
    using sdp_callback_t = std::function<void(RTCConnection&, std::string)>;

    friend class RTCConnectionManager;

    void async_send(std::unique_ptr<char[]> data, size_t size) override;
    uint16_t listen_port() const override { return 0; }
    std::optional<ConnectRequest> connect_request() const override { return {}; }

    struct Connect {
    };
    struct Accept {
        OneIpSdp& sdp;
    };

public:
    [[nodiscard]] static std::shared_ptr<RTCConnection> connect_new(std::weak_ptr<Eventloop>, sdp_callback_t cb, const IP&);
    [[nodiscard]] static std::shared_ptr<RTCConnection> accept_new(std::weak_ptr<Eventloop> eventloop, sdp_callback_t cb, OneIpSdp sdp);
    RTCConnection(bool isInbound, std::weak_ptr<Eventloop>, IP ip, variant_t data);
    RTCConnection(const RTCConnection&) = delete;
    RTCConnection(RTCConnection&&) = delete;
    virtual bool is_native() const override { return true; }
    std::shared_ptr<ConnectionBase> get_shared() override
    {
        return shared_from_this();
    }
    virtual ConnectionVariant get_shared_variant() override
    {
        return shared_from_this();
    }
    std::weak_ptr<ConnectionBase> get_weak() override
    {
        return weak_from_this();
    }
    Sockaddr peer_addr() const override { return { sockAddr }; }
    // RTCSockaddr connection_peer_addr_native() const ;
    // { return connectRequest.address; }
    bool inbound() const override { return isInbound; };

    void close(int errcode) override;
    [[nodiscard]] std::optional<Error> set_sdp_answer(OneIpSdp);

private:

    void set_error(int error);
    void if_not_closed(auto lambda);
    void notify_closed();
    void set_data_channel_proxied(std::shared_ptr<rtc::DataChannel>);
    bool closed() { return errcode != 0; }
private: // maybe proxied functions
    void init_proxied(sdp_callback_t&& sdpCallback);
    void send_proxied(std::string);

private:
    bool isInbound;

public:
    bool closeAfterConnected { false }; // close connection after verification
private:
    WebRTCSockaddr sockAddr;
    std::weak_ptr<Eventloop> eventloop;

    variant_t data;
    std::shared_ptr<rtc::DataChannel> dc;

    std::recursive_mutex errcodeMutex;
    int errcode { 0 };
    std::unique_ptr<rtc::PeerConnection> pc;
};
