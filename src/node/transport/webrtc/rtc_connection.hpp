#pragma once
#include "../connection_base.hpp"
#include "eventloop/types/conref_declaration.hpp"
#include "eventloop/types/rtc/registry_type.hpp"
#include <list>
#include <memory>

class Eventloop;

namespace rtc {
class DataChannel;
class PeerConnection;

}

namespace rtc_state {
class Connections;

// RTC state related data stored in each connection
class RTCConnectionData {
    friend class Connections;

private:
    registry_t::iterator rtcRegistryIter;
};
}

class RTCConnection final : public ConnectionBase, public std::enable_shared_from_this<RTCConnection>, public rtc_state::RTCConnectionData {

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

public:
    [[nodiscard]] static std::shared_ptr<RTCConnection> connect_new(Eventloop&, sdp_callback_t cb, const IP&, uint64_t verificationConId = 0);
    // feeler connection;
    [[nodiscard]] static std::shared_ptr<RTCConnection> connect_new_verification(Eventloop& e, sdp_callback_t cb, const IP& ip, uint64_t verificationConId)
    {
        return connect_new(e, std::move(cb), ip, verificationConId);
    }
    [[nodiscard]] static std::shared_ptr<RTCConnection> accept_new(Eventloop& eventloop, sdp_callback_t cb, OneIpSdp sdp, uint64_t verificationConId = 0);
    [[nodiscard]] static std::shared_ptr<RTCConnection> accept_new_verification(Eventloop& e, sdp_callback_t cb, OneIpSdp sdp, uint64_t verificationConId)
    {
        return accept_new(e, std::move(cb), std::move(sdp), verificationConId);
    }
    RTCConnection(bool isInbound, uint64_t verificationConId, std::weak_ptr<Eventloop>, IP ip, variant_t data);
    RTCConnection(const RTCConnection&) = delete;
    RTCConnection(RTCConnection&&) = delete;
    std::shared_ptr<ConnectionBase> get_shared() override
    {
        return shared_from_this();
    }

    template <typename callback_t>
    requires std::is_invocable_v<callback_t, IdentityIps&&>
    static void fetch_id(callback_t cb, bool stun = false);

    virtual ConnectionVariant get_shared_variant() override
    {
        return shared_from_this();
    }
    std::weak_ptr<ConnectionBase> get_weak() override
    {
        return weak_from_this();
    }
    Peeraddr peer_addr() const override { return { sockAddr }; }
    auto& native_peer_addr() const { return sockAddr ; }
    bool inbound() const override { return isInbound; };

    void close(Error) override;
    [[nodiscard]] std::optional<Error> set_sdp_answer(OneIpSdp);
    [[nodiscard]] auto& verification_con_id() { return verificationConId; }

private:
    [[nodiscard]] bool set_error(Error);
    void if_not_closed(auto lambda);
    void notify_closed();
    void set_data_channel_proxied(std::shared_ptr<rtc::DataChannel>);
    bool closed() { return error.has_value(); }

private: // maybe proxied functions
    void init_proxied(sdp_callback_t&& sdpCallback);
    void send_proxied(std::vector<std::byte>&&);

private:
    bool isInbound;
    uint64_t verificationConId { 0 }; // Nonzero specifies connection id of peer this RTC connection is verifying.
    WebRTCPeeraddr sockAddr;
    std::weak_ptr<Eventloop> eventloop;

    variant_t data;
    std::shared_ptr<rtc::DataChannel> dc;

    std::recursive_mutex errcodeMutex;
    std::optional<Error> error;
    std::unique_ptr<rtc::PeerConnection> pc;
};
