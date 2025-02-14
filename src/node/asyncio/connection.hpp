#pragma once
#include "version.hpp"
#include "communication/buffers/recvbuffer.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "conman.hpp"
#include "eventloop/types/conref_declaration.hpp"

class Connection final : public std::enable_shared_from_this<Connection> {
    struct TCP_t : public uv_tcp_t {
        std::shared_ptr<Connection> con;
        uv_stream_t* to_stream_ptr() { 
            return reinterpret_cast<uv_stream_t*>(static_cast<uv_tcp_t*>(this));
        }
        uv_handle_t* to_handle_ptr() { 
            return reinterpret_cast<uv_handle_t*>(static_cast<uv_tcp_t*>(this));
        }
        static TCP_t* from_handle_ptr(uv_handle_t* p){
            return static_cast<TCP_t*>(reinterpret_cast<uv_tcp_t*>(p));
        }
        TCP_t(uv_loop_t* loop, std::shared_ptr<Connection> p)
            : con(std::move(p))
        {
            assert(uv_tcp_init(loop, this) == 0);
        }
    };
    struct TimeoutTimer {
    private:
        struct Internal : public uv_timer_t {
            std::shared_ptr<Connection> con;
            std::shared_ptr<Internal> self;
            inline uv_handle_t* to_handle_ptr()
            {
                return reinterpret_cast<uv_handle_t*>(static_cast<uv_timer_t*>(this));
            }
            static inline Internal* from_handle_ptr(uv_handle_t* h)
            {
                return static_cast<Internal*>(reinterpret_cast<uv_timer_t*>(h));
            }
            static void on_close(uv_timer_t* handle){
                auto p { static_cast<Internal*>(handle) };
                p->con->close(ETIMEOUT);
                p->close();
            }
            Internal(Connection& c)
                :con(c.shared_from_this())
            {
                assert(uv_timer_init(c.conman.server.loop, this) == 0);
                assert(uv_timer_start(
                           this, on_close,
                           5000, 0)
                    == 0);
            }
            void close()
            {
                if (uv_is_closing(to_handle_ptr()))
                    return;
                assert(uv_timer_stop(this)==0);
                uv_close(to_handle_ptr(), [](uv_handle_t* handle) {
                    auto t { from_handle_ptr(handle) };
                    t->self = {};
                });
            }
        };
        std::weak_ptr<Internal> internal;

    public:
        bool expired()
        {
            return internal.expired();
        }
        void cancel()
        {
            if (auto p { internal.lock() }; p)
                p->close();
        }
        void start(Connection& c)
        {
            cancel();
            auto p { std::make_shared<Internal>(c) };
            internal = p;
            p->self = std::move(p);
        }
    };

private:
    // Connection counts its references and will eventually be destructed by
    // Conman using delete It must be created with new
    friend class Conman;
    struct Writebuffer {
        uv_write_t write_t;
        uv_buf_t buf;
        Writebuffer(std::unique_ptr<char[]>&& data, size_t size)
        {
            buf.len = size;
            buf.base = data.release();
        }
        ~Writebuffer() { delete[] buf.base; }
    };
    struct Handshakedata {
        std::array<uint8_t, 25> recvbuf; // 14 bytes for "WARTHOG GRUNT!" and 4
                                         // bytes for version + 4 extra bytes
                                         // (in case of outbound: + 2 bytes for
                                         //  sending port port + 1 byte for ack)

        bool waitForAck = false;
        size_t size(bool inbound) { return (inbound ? (waitForAck ? 25 : 24) : 22); }
        static constexpr const char connect_grunt[] = "WARTHOG GRUNT?";
        static constexpr const char accept_grunt[] = "WARTHOG GRUNT!";
        static constexpr const char connect_grunt_testnet[] = "TESTNET GRUNT?";
        static constexpr const char accept_grunt_testnet[] = "TESTNET GRUNT!";
        uint8_t pos = 0;
        bool handshakesent = false;
        NodeVersion version(bool inbound);
        uint16_t port(bool inbound)
        {
            assert(inbound);
            uint16_t tmp;
            memcpy(&tmp, recvbuf.data() + 22, 2);
            return ntoh16(tmp);
        }
    };

    //////////////////////////////
    // static members to be used as c callback functions in libuv
    static void alloc_caller(uv_handle_t* h, size_t s, uv_buf_t* b);
    static void write_caller(uv_write_t* w, int s);
    static void read_caller(uv_stream_t* s, ssize_t r, const uv_buf_t* b);
    static void connect_caller(uv_connect_t* req, int status);
    static void timeout_caller(uv_timer_t* handle);
    static void close_caller(uv_handle_t* handle);

    //////////////////////////////
    // members callbacks used for libuv
    void alloc_cb(size_t /*suggested_size*/, uv_buf_t* buf);
    void write_cb(int status);
    void read_cb(ssize_t nread, const uv_buf_t* /*buf*/);
    void connect_cb(int status);

    //////////////////////////////
    // mutex protected methods
    void async_send(std::unique_ptr<char[]> data, size_t size);

public:
    enum class State { CONNECTING,
        HANDSHAKE,
        CONNECTED,
        CLOSING,
    };
    std::vector<Rcvbuffer> extractMessages();
    void asyncsend(Sndbuffer&& msg);
    void async_close(int errcode);
    [[nodiscard]] EndpointAddress peer_address() { return peerAddress; }
    [[nodiscard]] EndpointAddress peer_endpoint() { return EndpointAddress { peerAddress.ipv4, peerEndpointPort }; }

    Connection(Conman& conman, bool inbound, std::optional<uint32_t> reconnectSeconds = {});
    ~Connection();
    Connection(const Connection&) = delete;
    Connection(Connection&&) = delete;
private:

    // closes connection and restarts after timeout if needed
    void close(int errcode);
    void send_handshake();
    void send_handshake_ack();
    int send_buffers();

    //////////////////////////////
    // Connection initialization
    int accept();
    int connect(EndpointAddress);
    int start_read();
    void eventloop_notify();

public:
    // data accessed by eventloop thread
    bool eventloop_registered = false;
    bool eventloop_erased = false;
    std::optional<uint32_t> reconnectSleep;
    const bool inbound;
    const uint64_t id;
    const uint32_t connected_since;
    Coniter dataiter;

    // methods not requiring mutex
    std::string to_string() const;

private:
    static uint64_t
        idcounter; // not synchronized, only create connections in one thread

    //////////////////////////////
    // data accessed by libuv thread
    Conman& conman;
    Rcvbuffer stagebuffer;
    std::unique_ptr<Handshakedata> handshakedata;
    NodeVersion peerVersion;
    int64_t logrow = -1;
    State state = State::CONNECTING;
    EndpointAddress peerAddress;
    uint16_t peerEndpointPort;
    std::shared_ptr<TCP_t> tcp;
    TimeoutTimer timeoutTimer;

    //////////////////////////////
    // Mutex locked members
    std::mutex mutex;
    std::list<Writebuffer> buffers; // FIFO queue
    std::set<EndpointAddress> reconnect;
    uint32_t bufferedbytes = 0;
    std::list<Writebuffer>::iterator buffercursor;
    std::vector<Rcvbuffer> readbuffers;
};
