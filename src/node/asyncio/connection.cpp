#include "connection.hpp"
#include "eventloop/eventloop.hpp"
#include "general/is_testnet.hpp"
#include "global/globals.hpp"
#include "version.hpp"

//////////////////////////////
// members callbacks used for libuv
//////////////////////////////

void Connection::write_cb(int status)
{
    {
        std::unique_lock<std::mutex> lock(mutex);
        bufferedbytes -= buffers.front().buf.len;
        buffers.erase(buffers.begin());
    }
    if (state != State::CONNECTED && state != State::HANDSHAKE)
        return;
    if (status) {
        close(status);
        return;
    }
}
void Connection::read_cb(ssize_t nread, const uv_buf_t* /*buf*/)
{
    if (state != State::CONNECTED && state != State::HANDSHAKE)
        return;
    if (nread < 0) {
        close(nread);
        return;
    }
    if (nread == 0)
        return;
    if (handshakedata) {
        auto& hb = *handshakedata;
        hb.pos += nread;
        if (inbound) {
            if (hb.pos == hb.size(inbound)) {
                if (hb.waitForAck) {
                    assert(hb.pos == 25);
                    spdlog::debug("Handshake valid, peer version {}", peerVersion.to_string());
                    timeoutTimer.cancel();
                    handshakedata.reset(nullptr);
                    state = State::CONNECTED;
                    if (reconnectSleep) {
                        reconnectSleep = 0;
                    }
                    eventloop_notify();
                } else {
                    assert(hb.pos == 24);
                    assert(handshakedata->handshakesent == false);
                    peerVersion = hb.version(inbound);
                    if (!peerVersion.initialized()) {
                        close(EHANDSHAKE);
                        return;
                    }
                    if (!peerVersion.compatible()) {
                        close(EVERSION);
                        return;
                    }
                    if (inbound) {
                        peerEndpointPort = hb.port(inbound);
                    }
                    send_handshake();
                    hb.waitForAck = true;
                }
            }
        } else {
            if (hb.pos == hb.size(inbound)) {
                peerVersion = hb.version(inbound);
                if (!peerVersion.initialized()) {
                    close(EHANDSHAKE);
                    return;
                }
                if (!peerVersion.compatible()) {
                    close(EVERSION);
                    return;
                }
                spdlog::debug("Handshake valid, peer version {}", peerVersion.to_string());
                if (handshakedata->handshakesent == false)
                    send_handshake();
                timeoutTimer.cancel();
                handshakedata.reset(nullptr);
                state = State::CONNECTED;
                if (reconnectSleep) {
                    reconnectSleep = 0;
                }
                eventloop_notify();
                send_handshake_ack();
            }
        }
        return;
    }
    const size_t newpos = stagebuffer.pos + nread;
    stagebuffer.pos = newpos;
    if (newpos == 10) {
        if (int r = stagebuffer.allocate_body()) {
            close(r);
            return;
        }
    }
    if (stagebuffer.body.bytes.size() != 0 && stagebuffer.pos == 8 + stagebuffer.body.bytes.size()) {
        if (stagebuffer.finished()) {
            spdlog::debug("Received complete message");

            {
                std::unique_lock<std::mutex> lock(mutex);
                readbuffers.push_back(std::move(stagebuffer));
                stagebuffer.pos = 0;
            }
            eventloop_notify();
        } else {
            stagebuffer.realloc();
        }
    }
}
void Connection::alloc_cb(size_t /*suggested_size*/, uv_buf_t* buf)
{
    if (handshakedata) {
        buf->base = (char*)handshakedata->recvbuf.data() + handshakedata->pos;
        buf->len = handshakedata->size(inbound) - handshakedata->pos;
        return;
    }
    if (stagebuffer.pos < 10) {
        buf->base = (char*)stagebuffer.header + stagebuffer.pos;
        buf->len = 10 - stagebuffer.pos;
        return;
    }
    size_t offset = stagebuffer.pos - 8;
    buf->base = (char*)stagebuffer.body.bytes.data() + offset;
    buf->len = stagebuffer.body.bytes.size() - offset;
}
void Connection::connect_cb(int status)
{
    if (status < 0) {
        close(status);
        return;
    }
    state = State::HANDSHAKE;
    conman.perIpCounter.insert(peerAddress.ipv4);

    if ((status = start_read()))
        close(status);
    else
        send_handshake();
}

uint64_t Connection::idcounter = 1; // global counter of ids

Connection::Connection(Conman& conman, bool inbound, std::optional<uint32_t> reconnectSeconds)
    : reconnectSleep(reconnectSeconds)
    , inbound(inbound)
    , id(idcounter++)
    , connected_since(now_timestamp())
    , conman(conman)
    , handshakedata(new Handshakedata())
    , buffercursor(buffers.end())
{
    if (idcounter == 0)
        idcounter = 1; // id shall never be 0
}
Connection::~Connection()
{
}

NodeVersion Connection::Handshakedata::version(bool inbound)
{ // return value 0 indicates error
    if (is_testnet()) {
        if (memcmp(recvbuf.data(), (inbound ? connect_grunt_testnet : accept_grunt_testnet), 14) != 0)
            return NodeVersion::from_uint32_t(0);
    } else {
        if (memcmp(recvbuf.data(), (inbound ? connect_grunt : accept_grunt), 14) != 0)
            return NodeVersion::from_uint32_t(0);
    }
    uint32_t tmp;
    memcpy(&tmp, recvbuf.data() + 14, 4);
    return NodeVersion::from_uint32_t(hton32(tmp));
}

void Connection::send_handshake()
{
    char* data = new char[24];
    if (is_testnet()) {
        memcpy(data, (inbound ? Handshakedata::accept_grunt_testnet : Handshakedata::connect_grunt_testnet), 14);
    } else {
        memcpy(data, (inbound ? Handshakedata::accept_grunt : Handshakedata::connect_grunt), 14);
    }
    uint32_t nver{hton32(NodeVersion::our_version().to_uint32())};
    memcpy(data + 14, &nver, 4);
    memset(data + 18, 0, 4);
    if (!inbound) {
        uint16_t portBe = hton16(conman.bindAddress.port);
        memcpy(data + 22, &portBe, 2);
        async_send(std::unique_ptr<char[]>(data), 24);
    } else {
        async_send(std::unique_ptr<char[]>(data), 22);
    }
    handshakedata->handshakesent = true;
}
void Connection::send_handshake_ack()
{
    char* data = new char[1];
    memcpy(data, "\0", 1);
    async_send(std::unique_ptr<char[]>(data), 1);
}

int Connection::send_buffers()
{
    std::unique_lock<std::mutex> lock(mutex);
    assert(tcp);
    while (buffercursor != buffers.end()) {
        buffercursor->write_t.data = this;
        if (int r = uv_write(&buffercursor->write_t, tcp->to_stream_ptr(),
                &buffercursor->buf, 1, [](uv_write_t* req, int status) {
                    Connection& con = (*reinterpret_cast<Connection*>(req->data));
                    con.write_cb(status);
                }))
            return r;
        ++buffercursor;
    }
    return 0;
}

int Connection::accept()
{
    int i;
    auto tmp { std::make_shared<TCP_t>(conman.server.loop, shared_from_this()) };
    if ((i = uv_accept((uv_stream_t*)&conman.server, tmp->to_stream_ptr())))
        return i;
    tcp = std::move(tmp);

    // extract ip and port
    sockaddr_storage storage;
    int alen = sizeof(storage);
    if (i = uv_tcp_getpeername(&*tcp, (struct sockaddr*)&storage, &alen); i != 0)
        return i;
    if (storage.ss_family != AF_INET)
        return EREFUSED;
    sockaddr_in* addr_i4 = (struct sockaddr_in*)&storage;
    peerAddress.ipv4 = IPv4(ntoh32(uint32_t(addr_i4->sin_addr.s_addr)));
    peerAddress.port = addr_i4->sin_port;
    connection_log().info("{} new incoming", to_string());
    if (!conman.count(peerAddress.ipv4)) {
        return EMAXCONNECTIONS;
    };
    state = State::HANDSHAKE;
    return 0;
}

int Connection::start_read()
{
    if (state != State::HANDSHAKE) {
        assert(state == State::CLOSING);
        return 0;
    }
    handshakedata.reset(new Handshakedata());

    timeoutTimer.start(*this);
    if (int i = uv_read_start(
            tcp->to_stream_ptr(),
            [](uv_handle_t* handle, size_t suggested_size,
                uv_buf_t* buf) { // alloc_cb
                auto& tcp { (*static_cast<TCP_t*>(reinterpret_cast<uv_tcp_t*>(handle))) };
                tcp.con->alloc_cb(suggested_size, buf);
            },
            [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
                auto& tcp { (*static_cast<TCP_t*>(reinterpret_cast<uv_tcp_t*>(stream))) };
                tcp.con->read_cb(nread, buf);
            }))
        return i;
    return 0;
}

int Connection::connect(EndpointAddress a)
{
    auto tmp { std::make_shared<TCP_t>(conman.server.loop, shared_from_this()) };
    peerAddress = a;
    peerEndpointPort = a.port;
    struct connect_t : public uv_connect_t {
        std::shared_ptr<Connection> pin;
        connect_t(std::shared_ptr<Connection> pin)
            : pin(std::move(pin))
        {
        }
    };
    auto p = new connect_t(shared_from_this());
    auto addr { a.sock_addr() };
    connection_log().info("{} connecting ", to_string());
    if (int i = uv_tcp_connect(p, &*tmp, (const sockaddr*)&addr,
            [](uv_connect_t* req, int status) {
                auto p = static_cast<connect_t*>(req);
                p->pin->connect_cb(status);
                delete p;
            });
        i != 0) {
        delete p;
        return i;
    } else {
        tcp = std::move(tmp);
        state = State::CONNECTING;
        return 0;
    }
}

void Connection::close(int errcode)
{
    if (state == State::CLOSING)
        return;

    if (state != State::CONNECTING) {
        conman.perIpCounter.erase(peerAddress.ipv4);
    }

    if (errors::leads_to_ban(errcode)) {
        if (reconnectSleep) {
            reconnectSleep = 60 * 60; // do not connect for 1 hour
        }
    }
    if (!inbound && (state == State::CONNECTING || state == State::HANDSHAKE)) {
        global().pel->async_report_failed_outbound(peerAddress);
    }

    state = State::CLOSING;
    connection_log().info("{} closed: {} ({})",
        to_string(), errors::err_name(errcode), errors::strerror(errcode));
    conman.peerServer.async_register_close(peerAddress.ipv4, errcode, logrow);
    global().pel->async_erase(shared_from_this(), errcode);
    std::unique_lock<std::mutex> lock(mutex);

    // delete unsent buffers
    while (buffercursor != buffers.end()) {
        bufferedbytes -= buffercursor->buf.len;
        buffers.erase(buffercursor++);
    }

    if (tcp)
        uv_close(tcp->to_handle_ptr(), [](uv_handle_t* handle) {
            TCP_t::from_handle_ptr(handle)->con = {};
        });
    timeoutTimer.cancel();

    conman.async_delete(shared_from_this());
}

//////////////////////////////
// BELOW METHODS CALLED FROM BOTH UV-THREAD AND MESSAGE PROCESSING THREAD
//////////////////////////////

// CALLED BY OTHER THREAD
std::vector<Rcvbuffer> Connection::extractMessages()
{
    std::unique_lock<std::mutex> lock(mutex);
    std::vector<Rcvbuffer> tmp;
    tmp.swap(readbuffers);
    return tmp;
}

std::string Connection::to_string() const
{
    return "(" + std::to_string(id) + ")" + (inbound ? "← " : "→ ") + peerAddress.to_string();
}

// CALLED BY OTHER THREAD
void Connection::async_send(std::unique_ptr<char[]> data, size_t size)
{
    std::unique_lock<std::mutex> lock(mutex);
    if (state == State::CLOSING)
        return;
    buffers.emplace_back(std::move(data), size);
    bufferedbytes += size;
    if (buffercursor == buffers.end())
        --buffercursor;
    if (bufferedbytes >= MAXBUFFER) {
        async_close(EBUFFERFULL);
    }
    conman.async_send(shared_from_this());
}

void Connection::asyncsend(Sndbuffer&& msg)
{
    msg.writeChecksum();
    async_send(std::move(msg.ptr), msg.fullsize());
}

void Connection::async_close(int32_t errcode) { conman.async_close(shared_from_this(), errcode); }

void Connection::eventloop_notify()
{
    global().pel->async_process(shared_from_this());
}
