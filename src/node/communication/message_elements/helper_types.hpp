#pragma once
#include "block/chain/height.hpp"
#include "block/header/header.hpp"
#include "general/descriptor.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Writer;
class Reader;

struct String16 {
    String16();
    String16(std::string data);
    String16(Reader& r);

    size_t byte_size() const;
    friend Writer& operator<<(Writer&, const String16&);

    std::string data;
};

class RTCPeers {
    struct Element {
        uint32_t key;
        String16 sdp; // webrtc sdp
    };

public:
    RTCPeers();
    RTCPeers(Reader& r);
    std::vector<Element> entries;
    size_t byte_size() const;
    friend Writer& operator<<(Writer&, const RTCPeers&);
};

namespace messages {

template <typename T, typename len_t>
struct VectorLentype : public std::vector<T> {
    static constexpr size_t maxlen = len_t(-1);
    VectorLentype(std::vector<T> v)
        : std::vector<T>(std::move(v))
    {
    }
    VectorLentype(Reader&);
    void push_back(T t);
    size_t bytesize() const;
};

template <typename T>
struct VectorRest : public std::vector<T> {
    using std::vector<T>::vector;
    VectorRest(std::vector<T> v)
        : std::vector<T>(std::move(v))
    {
    }
    VectorRest(Reader&);
    size_t bytesize() const;
};

template <typename T>
using Vector8 = VectorLentype<T, uint8_t>;

template <typename T>
using Vector16 = VectorLentype<T, uint16_t>;

template <typename T>
class Optional : public std::optional<T> {
public:
    using std::optional<T>::optional;
    size_t byte_size() const;
    Optional(Reader&);
    Optional(std::optional<T> o)
        : std::optional<T>(std::move(o))
    {
    }
};

}

struct CurrentAndRequested {
    CurrentAndRequested() { }
    CurrentAndRequested(Reader& r);
    friend Writer& operator<<(Writer&, const CurrentAndRequested&);
    size_t byte_size() const;
    std::optional<Header> requested;
    std::optional<Header> current;
};

struct BatchSelector {
    Descriptor descriptor;
    NonzeroHeight startHeight;
    NonzeroHeight end() const { return startHeight + length; }
    uint16_t length;
    BatchSelector(Descriptor d, NonzeroHeight s, uint16_t l)
        : descriptor(d)
        , startHeight(s)
        , length(l) {};
    BatchSelector(Reader& r);
};

template <int32_t parseHeightZeroError>
class NonzeroHeightParser : public NonzeroHeight {
public:
    using NonzeroHeight::NonzeroHeight;
    NonzeroHeightParser(NonzeroHeight& h)
        : NonzeroHeight(h)
    {
    }
    NonzeroHeightParser(Reader& r)
        : NonzeroHeight(Height(r).nonzero_throw(parseHeightZeroError))
    {
    }
};
