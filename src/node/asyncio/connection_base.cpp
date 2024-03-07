#include "connection_base.hpp"
#include "asyncio/conman.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "eventloop/eventloop.hpp"
#include "general/is_testnet.hpp"
#include "global/globals.hpp"
#include "peerserver/peerserver.hpp"
#include "uvw.hpp"
#include "version.hpp"
#include <chrono>

namespace {
std::mutex statechangeMutex;
std::atomic<uint64_t> connectionCounter { 1 }; // global counter of ids
}
ConnectionBase::ConnectionBase(EndpointAddress peer, bool inbound)
    : peerserver::Connection(peer, inbound)
    , id(connectionCounter++)
    , createdAt(std::chrono::steady_clock::now())
    , createdAtSystem(std::chrono::system_clock::now())
{
}

std::string ConnectionBase::to_string() const
{
    return "(" + std::to_string(id) + ")" + (inbound ? "← " : "→ ") + peer.to_string();
}

EndpointAddress ConnectionBase::peer_endpoint() const
{
    if (inbound) {
        // on inbound connection take the port the peer claims to listen on
        return EndpointAddress { peer.ipv4,
            std::get<MessageState>(state).handshakeData.port.value() };
    } else {
        // on outbound connection the port is the correct peer endpoint port
        return peer;
    }
};

std::vector<Rcvbuffer> ConnectionBase::pop_messages()
{
    std::lock_guard l(statechangeMutex);
    return std::move(std::get<MessageState>(state).readbuffers);
}

void ConnectionBase::handshake_timer_start()
{
    using namespace std::chrono_literals;
    // start handshake timeout timer on libuv thread
    global().conman->async_call([con = get_shared()]() {
        {
            std::lock_guard l(statechangeMutex);
            if (std::holds_alternative<HandshakeState>(con->state)) {
                auto& handshakeState { std::get<HandshakeState>(con->state) };
                auto timer { (*global().uv_loop)->resource<uvw::timer_handle>() };
                timer->start(5000ms, 0ms); // timeout for handshake
                timer->on<uvw::timer_event>([con = std::move(con)](
                                                auto&, uvw::timer_handle& timer) {
                    con->handshake_timer_expired();
                    timer.close();
                });
                handshakeState.expire_timer = std::move(timer);
            }
        }
    });
}

void ConnectionBase::handshake_timer_expired()
{
    std::lock_guard l(statechangeMutex);
    if (std::holds_alternative<HandshakeState>(state)) {
        close(ETIMEOUT);
        std::get<HandshakeState>(state).expire_timer->reset();
    }
}

void ConnectionBase::on_message(std::span<uint8_t> s)
{
    try {
        while (s.size() > 0) {
            s = std::visit([&](auto& mode) { return process_message(s, mode); }, state);
        }
    } catch (Error& e) {
        close(e);
        return;
    }
}

std::span<uint8_t> ConnectionBase::process_message(std::span<uint8_t>, std::monostate&)
{
    assert(false); // this should not happen because on_connected should be called first
}
std::span<uint8_t> ConnectionBase::process_message(std::span<uint8_t> data, HandshakeState& p)
{
    auto r { p.remaining(inbound) };
    auto s { data.size() };
    if (r <= s) {
        std::ranges::copy(data, p.data() + p.pos);
        p.pos += s;
        if (r == s) {
            if (inbound) {
                send_handshake();
                std::lock_guard l(statechangeMutex);
                state = AckState(p.parse(inbound));
            } else {
                send_handshake_ack();
                std::lock_guard l(statechangeMutex);
                state.emplace<MessageState>(p.parse(inbound));
            }
        }
        return {};
    } else {
        throw Error(EHANDSHAKE);
    }
}

std::span<uint8_t> ConnectionBase::process_message(std::span<uint8_t> s, AckState& as)
{
    std::lock_guard l(statechangeMutex);
    state.emplace<MessageState>(as);
    return { s.begin() + 1, s.end() }; // skip 1 byte
}

std::span<uint8_t> ConnectionBase::process_message(std::span<uint8_t> s, MessageState& m)
{
    auto& b { m.currentBuffer };
    auto cursor_src { s.begin() };

    if (b.pos < 10) {
        size_t rest_dst { 10 - b.pos };
        size_t rest_src(s.end() - cursor_src);
        size_t n { std::min(rest_dst, rest_src) };
        std::copy(cursor_src, cursor_src + n, &b.header[b.pos]);
        cursor_src += n;
        b.pos += n;
        if (b.pos == 10)
            b.allocate_body();
    }
    if (b.pos >= 10) {
        size_t rest_dst { b.bsize + 8 - b.pos };
        size_t rest_src(s.end() - cursor_src);
        size_t n { std::min(rest_dst, rest_src) };
        std::copy(cursor_src, cursor_src + n, std::back_inserter(b.body.bytes));
        cursor_src += n;
        b.pos += n;
        if (rest_dst = b.bsize + 8 - b.pos; rest_dst == 0) {
            std::lock_guard l(statechangeMutex);
            m.readbuffers.push_back(std::move(m.currentBuffer));
            m.currentBuffer.clear();
            global().core->async_process(get_shared());
        }
    }
    return { cursor_src, s.end() };
}

void ConnectionBase::send(Sndbuffer&& msg)
{
    msg.writeChecksum();
    async_send(std::move(msg.ptr), msg.fullsize());
}

void ConnectionBase::on_close(const CloseState& cs)
{
    global().core->erase(get_shared());
    // l->async_register_close(peerAddress.ipv4, errcode, logrow); // TODO: logrow
    global().peerServer->async_register_close(get_shared(), cs.error); // TODO: use failed outbound
}

HandshakeState::~HandshakeState()
{
    std::shared_ptr<uvw::timer_handle> t { std::move(expire_timer) };
    if (t) {
        // cancel timer on libuv thread
        global().conman->async_call([t]() {
            t->close();
        });
    }
}

auto HandshakeState::parse(bool inbound) -> Parsed
{ // verify handshake data
    if (is_testnet()) {
        if (memcmp(recvbuf.data(), (inbound ? connect_grunt_testnet : accept_grunt_testnet), 14) != 0)
            throw Error(EHANDSHAKE);
    } else {
        if (memcmp(recvbuf.data(), (inbound ? connect_grunt : accept_grunt), 14) != 0)
            throw Error(EHANDSHAKE);
    }
    uint32_t tmp;
    memcpy(&tmp, recvbuf.data() + 14, 4);
    Parsed p {
        .version = hton32(tmp)
    };
    if (inbound) {
        uint16_t tmp;
        memcpy(&tmp, recvbuf.data() + 22, 2);
        p.port = ntoh16(tmp);
    }
    return p;
}

void ConnectionBase::send_handshake()
{
    char* data = new char[24];
    if (is_testnet()) {
        memcpy(data, (inbound ? HandshakeState::accept_grunt_testnet : HandshakeState::connect_grunt_testnet), 14);
    } else {
        memcpy(data, (inbound ? HandshakeState::accept_grunt : HandshakeState::connect_grunt), 14);
    }
    uint32_t nver = hton32(version);
    memcpy(data + 14, &nver, 4);
    memset(data + 18, 0, 4);
    if (!inbound) {
        uint16_t portBe = hton16(listen_port());
        memcpy(data + 22, &portBe, 2);
        async_send(std::unique_ptr<char[]>(data), 24);
    } else {
        async_send(std::unique_ptr<char[]>(data), 22);
    }
}

void ConnectionBase::send_handshake_ack()
{
    char* data = new char[1];
    memcpy(data, "\0", 1);
    async_send(std::unique_ptr<char[]>(data), 1);
}

void ConnectionBase::on_connected()
{
    if (!inbound) {
        send_handshake();
    }
    std::lock_guard l(statechangeMutex);
    state = HandshakeState();
}
