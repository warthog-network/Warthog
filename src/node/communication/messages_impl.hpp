#include "communication/buffers/sndbuffer.hpp"
#include "general/writer.hpp"
#include "message_elements/byte_size.hpp"
#include "message_elements/helper_types_impl.hpp"
#include "message_elements/packer_impl.hpp"
#include "messages.hpp"

namespace {
struct MessageWriter {
    MessageWriter(uint8_t msgtype, size_t msglen)
        : sb(msgtype, msglen)
        , writer(sb.msg())
    {
    }
    operator Sndbuffer()
    {
        assert(writer.remaining() == 0);
        return std::move(sb);
    }
    MessageWriter& operator<<(bool b)
    {
        writer << (b ? uint8_t(1) : uint8_t(0));
        return *this;
    }
    template <typename T>
    MessageWriter& operator<<(const T& b)
    {
        writer << b;
        return *this;
    }
    // MessageWriter& operator<<(const Range& r)
    // {
    //     writer << r;
    //     return *this;
    // }

    MessageWriter& operator<<(const Worksum& worksum)
    {
        for (uint32_t fr : worksum.getFragments()) {
            *this << fr;
        }
        return *this;
    }

private:
    Sndbuffer sb;
    Writer writer;
};
} // namespace

template <uint8_t M>
MessageWriter MsgCode<M>::gen_msg(size_t len)
{
    return { msgcode, len };
}
