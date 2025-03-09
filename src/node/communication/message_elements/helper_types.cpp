#include "helper_types.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include <cassert>

RandNonce::RandNonce()
    : WithNonce { uint32_t(rand()) }
{
}

size_t RTCPeers::byte_size() const
{
    size_t sum = 1; // 1 for total number
    for (auto& s : entries)
        sum += 4 + s.sdp.byte_size();
    return sum;
}

Writer& operator<<(Writer& w, const RTCPeers& p)
{
    w << (uint8_t)(p.entries.size());
    for (auto& s : p.entries)
        w << s.key << s.sdp;
    return w;
}

RTCPeers::RTCPeers(Reader& r)
{
    size_t n = r.uint8();
    for (size_t i = 0; i < n; ++i) {
        entries.push_back({ r.uint32(), r });
    }
}

String16::String16(std::string data)
    : data(std::move(data))
{
    assert(data.size() <= 0xFFFF);
}

String16::String16(Reader& r)
{
    size_t n = r.uint16();
    data = r.take_string_view(n);
}

size_t String16::byte_size() const
{
    return 2 + data.size();
}

Writer& operator<<(Writer& w, const String16& s)
{
    return w << (uint16_t)(s.data.size())
             << s.data;
}

CurrentAndRequested::CurrentAndRequested(Reader& r)
{
    uint8_t type = r.uint8();
    if (type > 3)
        throw Error(EINV_PROBE);
    if ((type & 1) > 0) {
        requested = r.view<HeaderView>();
    }
    if ((type & 2) > 0) {
        current = r.view<HeaderView>();
    }
}

Writer& operator<<(Writer& w, const CurrentAndRequested& car)
{
    uint8_t type = 0;
    if (car.requested.has_value())
        type += 1;

    if (car.current.has_value())
        type += 2;

    w << type;
    if (car.requested.has_value())
        w << *car.requested;
    if (car.current.has_value())
        w << *car.current;
    return w;
}

size_t CurrentAndRequested::byte_size() const
{
    size_t s = 1;
    if (requested.has_value())
        s += requested.value().byte_size();
    if (current.has_value())
        s += current.value().byte_size();
    return s;
}

Writer& operator<<(Writer& w, const BatchSelector& s)
{
    return w << s.descriptor
             << s.startHeight
             << s.length;
}
BatchSelector::BatchSelector(Reader& r)
    : descriptor(r)
    , startHeight(Height(r).nonzero_throw(EBATCHHEIGHT))
    , length(r)
{
    if (length == 0) 
        throw Error(EBLOCKRANGE);
}
