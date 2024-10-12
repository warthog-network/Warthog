#include "connect_request.hpp"
#include "ws_urladdr.hpp"
#include "transport/connection_base.hpp"
#include "transport/ws/browser/emscripten_wsconnection.hpp"
#include <list>

class WSSession;
class WSConnectionManager;


class WSConnection final : public ConnectionBase, public std::enable_shared_from_this<WSConnection> {
    void async_send(std::unique_ptr<char[]> data, size_t size) override;
    uint16_t listen_port() const override;
    std::optional<ConnectRequest> connect_request() const override;
    struct CreationToken { };

    friend void start_connection(const WSUrladdr& r);
    friend class WSSession;

public:
    WSConnection(CreationToken, const WSBrowserConnectRequest& r, EmscriptenWSConnection&&);

    [[nodiscard]] static bool connect(const WSBrowserConnectRequest& r);
    WSConnection(const WSConnection&) = delete;
    WSConnection(WSConnection&&) = delete;
    std::shared_ptr<ConnectionBase> get_shared() override
    {
        return shared_from_this();
    }
    ConnectionVariant get_shared_variant() override
    {
        return shared_from_this();
    }
    std::weak_ptr<ConnectionBase> get_weak() override
    {
        return weak_from_this();
    }

    auto& connection_peer_addr_native() const { return connectRequest.address(); }
    Peeraddr peer_addr() const override { return { connection_peer_addr_native() }; }
    bool inbound() const override;

private:
    int on_open();
    int on_error();
    int on_close(EmscriptenWSConnection::CloseInfo);
    int on_message(EmscriptenWSConnection::Message);
    void close(Error reason) override;
    void notify_closed(Error reason);

    const WSBrowserConnectRequest connectRequest;
    EmscriptenWSConnection emscriptenConnection;
    std::shared_ptr<WSConnection> self;

    std::mutex close_mutex;
    bool closed = false;
};
