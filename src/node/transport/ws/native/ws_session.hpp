#pragma once
#include <list>
#include <memory>
#include <span>
extern "C" {
struct lws;
}

class WSConnection;

struct WSSession {
private:
    class CreationToken { };

public:
    struct Msg {
        std::unique_ptr<char[]> data;
        size_t len;
    };

    WSSession(CreationToken, bool inbound, lws* wsi = nullptr);
    [[nodiscard]] static std::shared_ptr<WSSession> make_new(bool inbound, lws* wsi = nullptr);

    struct CurrentMsg {
        Msg* m = nullptr;
        size_t cursor = 0;

        size_t remaining() { return m->len - cursor; }
    };
    void queue_write(Msg msg);

    void update_current()
    {
        if (current.m == nullptr) {
            current.m = &messages.front();
            current.cursor = 0;
        }
    }
    void close(int32_t reason);
    void on_close(int32_t reason);
    void on_connected();

    int receive(std::span<uint8_t> data);
    int write();

    std::shared_ptr<WSConnection> connection;

private:
    std::list<Msg> messages;
    const bool inbound;
    bool closing = false;
    CurrentMsg current;
    static constexpr size_t chunkSize = 1024;

public:
    lws* wsi = nullptr;
};
