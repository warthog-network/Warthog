#pragma once
#include "communication/buffers/recvbuffer.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "conman.hpp"
#include "eventloop/types/conref_declaration.hpp"

class Connection final {
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
        static constexpr const char accept_grunt_testnet[] =  "TESTNET GRUNT!";
        uint8_t pos = 0;
        bool handshakesent = false;
        uint32_t version(bool inbound)
        { // return value 0 indicates error
            if (memcmp(recvbuf.data(), (inbound ? connect_grunt : accept_grunt), 14) != 0)
                return 0;
            uint32_t tmp;
            memcpy(&tmp, recvbuf.data() + 14, 4);
            return hton32(tmp);
        }
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
    void async_send(std::unique_ptr<char[]>&& data, size_t size);

public:
    enum class State { CONNECTING,
        HANDSHAKE,
        CONNECTED,
        CLOSING,
    };
    std::vector<Rcvbuffer> extractMessages();
    void eventloop_unref(const char* tag);
    void asyncsend(Sndbuffer&& msg);
    void async_close(int errcode);
    [[nodiscard]] EndpointAddress peer_address() { return peerAddress; }
    [[nodiscard]] EndpointAddress peer_endpoint() { return EndpointAddress { peerAddress.ipv4, peerEndpointPort }; }

private:
    void unref(const char* tag);
    Connection(Conman& conman, bool inbound, std::optional<uint32_t> reconnectSeconds = {});
    ~Connection();
    Connection(const Connection&) = delete;
    Connection(Connection&&) = delete;

    // closes connection and restarts after timeout if needed
    void close(int errcode);
    void addref(const char* tag);
    void addref_locked(const char* tag);
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
    bool eventloop_erased = false;
    bool eventloop_registered = false;
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
    uint32_t peerVersion;
    int64_t logrow = -1;
    State state;
    bool eventloopref = false; // whether the eventloop took notice of this connection
    EndpointAddress peerAddress;
    uint16_t peerEndpointPort;
    uv_tcp_t tcp;
    uv_timer_t timer;

    //////////////////////////////
    // Mutex locked members
    std::mutex mutex;
    int refcount { 0 };
    std::list<Writebuffer> buffers; // FIFO queue
    std::set<EndpointAddress> reconnect;
    uint32_t bufferedbytes = 0;
    std::list<Writebuffer>::iterator buffercursor;
    std::vector<Rcvbuffer> readbuffers;
};
