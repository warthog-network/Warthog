#pragma once
#include "general/byte_order.hpp"
#include <cassert>
#include <cstring>
#include <memory>
#include <span>

class Sndbuffer {
public:
    const uint32_t len;
    std::unique_ptr<char[]> ptr;
    Sndbuffer(uint8_t msgtype, uint32_t msglen)
        : len(msglen + 10)
        , ptr(new char[len])
    {
        ptr[8] = 0;
        ptr[9] = msgtype;
        uint32_t n = hton32(len - 8);
        memcpy(ptr.get(), &n, 4);
    }
    void writeChecksum();
    std::span<uint8_t> msg()
    {
        return { msgdata(), msgsize() };
    }
    size_t fullsize() { return len; }

private:
    uint8_t* msgdata() { return reinterpret_cast<uint8_t*>(ptr.get() + 10); };
    size_t msgsize() { return len - 10; }
};
