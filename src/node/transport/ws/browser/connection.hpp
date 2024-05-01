#include "../connect_request.hpp"
#include "transport/connection_base.hpp"
#include "transport/ws/browser/emscripten_wsconnection.hpp"
#include <list>

class WSSession;
class WSConnectionManager;

class WSConnection final : public std::enable_shared_from_this<WSConnection>, public ConnectionBase {
    void async_send(std::unique_ptr<char[]> data, size_t size) override;
    uint16_t listen_port() const override;
    ConnectRequest connect_request() const override;
    struct CreationToken {};

    friend void start_connection(const WSConnectRequest& r);
    friend class WSSession;
public:
    WSConnection(CreationToken, const WSConnectRequest& r, EmscriptenWSConnection&&);

    [[nodiscard]] static std::shared_ptr<WSConnection> make_new(const WSConnectRequest& r);
    WSConnection(const WSConnection&) = delete;
    WSConnection(WSConnection&&) = delete;
    std::shared_ptr<ConnectionBase> get_shared() override
    {
        return shared_from_this();
    }
    std::weak_ptr<ConnectionBase> get_weak() override
    {
        return weak_from_this();
    }

    Sockaddr connection_peer_addr() const override { return { connection_peer_addr_native() }; }
    WSSockaddr connection_peer_addr_native() const { return connectRequest.address; }
    Sockaddr claimed_peer_addr() const override;
    bool inbound() const override;

private:
    int on_open();
    int on_error();
    int on_close(EmscriptenWSConnection::CloseInfo);
    int on_message(EmscriptenWSConnection::Message);
    void start_read() override;
    void close(int reason) override;
    void notify_closed(int32_t reason);

    const WSConnectRequest connectRequest;
    EmscriptenWSConnection emscriptenConnection;
    std::shared_ptr<WSConnection> self;

    std::mutex close_mutex;
    bool closed = false;
};
