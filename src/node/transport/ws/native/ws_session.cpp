#include "ws_session.hpp"
#include "spdlog/spdlog.h"
#include "transport/ws/native/connection.hpp"
#include <cstring>
#include <iostream>
extern "C" {
#include "libwebsockets.h"
}
using namespace std;

std::shared_ptr<WSSession> WSSession::make_new(bool inbound, lws* wsi)
{
    return std::make_shared<WSSession>(CreationToken {}, inbound, wsi);
}

void WSSession::queue_write(Msg msg)
{
    messages.push_back(std::move(msg));
    update_current();
    lws_callback_on_writable(wsi);
}

WSSession::WSSession(CreationToken, bool inbound, lws* wsi)
    : inbound(inbound)
    , wsi(wsi)
{
}

void WSSession::on_close(int32_t reason)
{
    if (closing)
        return;
    closing = true;
    if (connection)
        connection->on_close({ .error = reason });
}
void WSSession::close(int32_t reason)
{
    on_close(reason);
    assert(wsi);
    lws_set_timeout(wsi, (pending_timeout)1, LWS_TO_KILL_SYNC);
}

void WSSession::on_connected()
{
    assert(connection);
    connection->on_connected();
}

int WSSession::receive(std::span<uint8_t> data)
{
    connection->on_message(data);
    return 0;
}

int WSSession::write()
{
    if (!current.m)
        return 0;

    int r = (current.cursor == 0 ? LWS_WRITE_BINARY : LWS_WRITE_CONTINUATION);
    size_t remaining = current.remaining();
    bool msgDone = false;
    size_t n;
    if (remaining > chunkSize) {
        r |= LWS_WRITE_NO_FIN;
        n = chunkSize;
    } else {
        msgDone = true;
        n = remaining;
    }

    unsigned char* pData { (unsigned char*)current.m->data.get() + current.cursor };
    { // write
        int res;
        if (current.cursor >= LWS_PRE) {
            res = lws_write(wsi, pData, n, (lws_write_protocol)r);
        } else {
            uint8_t buf[chunkSize + LWS_PRE];
            memcpy(buf + LWS_PRE, pData, n);
            res = lws_write(wsi, buf + LWS_PRE, n, (lws_write_protocol)r);
        }
        if (res < (int)n)
            return -1;
    }

    current.cursor += n;
    if (msgDone) {
        messages.pop_front();
        if (messages.empty()) {
            current.m = nullptr;
            return 0;
        } else {
            current.m = &messages.front();
            current.cursor = 0;
        }
    }
    lws_callback_on_writable(wsi);
    return 0;
}
