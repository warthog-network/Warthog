#pragma once

#include "communication/messages.hpp"
#include "general/errors.hpp"
#include "general/reader.hpp"
#include <cstdint>
#include <cstring>

class Rcvbuffer {
    friend class ConnectionBase;
    friend class Reader;

public:
    operator Reader()
    {
        return Reader(body.msg());
    }
    std::vector<uint8_t>&& extractBody() { return std::move(body.bytes); }
    uint32_t bodysize()
    {
        return readuint32(header);
    }
    uint8_t type() { return header[9]; }
    Rcvbuffer() {};
    Rcvbuffer(Rcvbuffer&& buf)
    {
        memcpy(header, buf.header, sizeof(header));
        body = std::move(buf.body);
        pos = buf.pos;
        buf.pos = 0;
    }
    messages::Msg parse();

private: // private methods
    bool verify();
    void clear()
    {
        pos = 0;
        body.bytes.clear();
    }
    void allocate_body()
    {
        bsize = bodysize();
        // Check if message is valid. The message is encoded in the 2 message
        // type bytes header[8] and header[9]. For now, header[8] must be 0
        // because one byte header[9] is sufficient to cover all message cases.
        size_t sb = messages::size_bound(header[9]);
        if (header[8] != 0 || sb == 0)
            throw Error(EMSGTYPE);
        // Check if proposed bodysize is in valid limits
        if (bsize < 2 || bsize > 2 + sb) {
            throw Error(EMSGLEN);
        }
        // Now allocate
        body.bytes.resize(2);
        // Copy additional bytes into body (needed for checksum)
        body.bytes[0] = header[8];
        body.bytes[1] = header[9];
    }
    bool finished()
    {
        return bsize > 0 && bsize + 8 == pos;
    }

private: // private members
    uint8_t header[10]; // 4 bytes body size + 4 bytes checksum + 2 bytes message
                        // type,
    struct Body {
        std::vector<uint8_t> bytes;
        std::span<const uint8_t> msg() const { return std::span(bytes).subspan(2); };
    } body;
    size_t pos = 0;
    size_t bsize = 0;
};
