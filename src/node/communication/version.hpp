#pragma once
#include "version_def.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

class ProtocolVersion;
class NodeVersion {
    constexpr NodeVersion(uint32_t v)
        : version(v) { };

public:
    static constexpr NodeVersion parse_without_prefix(const char* s, size_t n)
    {
        auto make_exception { []() {
            return std::runtime_error("invalid node version");
        } };
        uint32_t v { 0 };
        size_t byte { 0 };
        size_t chunk { 0 };
        size_t digits { 0 };
        for (size_t i { 0 }; i < n; ++i) {
            char c { s[i] };
            if (c >= '0' && c <= '9') {
                digits += 1;
                byte *= 10;
                byte += c - '0';
                if (byte > 255)
                    throw make_exception();
            } else if (c == '.') {
                if (digits == 0)
                    throw make_exception();
                v <<= 8;
                v += byte;
                byte = 0;
                digits = 0;
                chunk += 1;
            } else
                throw make_exception();
        }
        if (chunk == 2 && digits != 0) {
            v <<= 8;
            v += byte;
            return { v };
        }
        throw make_exception();
    }
    constexpr NodeVersion(uint8_t major, uint8_t minor, uint8_t patch)
        : version((uint32_t(major) << 16) | (uint32_t(minor) << 8) | (uint32_t(patch)))
    {
    }
    auto operator<=>(const NodeVersion&) const = default;
    static constexpr NodeVersion our_version()
    {
        return { VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH };
    }

    friend class ProtocolVersion;
    static constexpr NodeVersion from_uint32_t(uint32_t v)
    {
        return { v };
    }

    bool compatible() const
    {
        return true; // initialized() && (minor() >= 8 || (minor() == 6 && patch() >= 21));
    }
    uint32_t raw() const { return version; }
    std::string to_string() const;
    bool initialized() const { return version != 0; }
    constexpr uint32_t to_uint32() const { return version; }
    ProtocolVersion protocol();
    uint8_t major() const { return 0xFF & (version >> 16); }
    uint8_t minor() const { return 0xFF & (version >> 8); }
    uint8_t patch() const { return 0xFF & version; }

private:
    uint32_t version { 0 };
};

inline constexpr NodeVersion operator""_v(const char* v, std::size_t n)
{
    return NodeVersion::parse_without_prefix(v, n);
}

class ProtocolVersion : protected NodeVersion {
    friend class NodeVersion;

private:
    ProtocolVersion(NodeVersion nv)
        : NodeVersion(nv) { };

public:
    bool v1() { return (*this < "0.5.0"_v || ((version >> 8) == 8) || ((version >> 8) == 6)) && *this <= "0.10.0"_v; }
    bool v2() { return !v1() && (*this <= "0.7.54"_v); }
    bool v3() { return !v1() && !v2() && *this <= "0.10.0"_v; }
    bool before_defi() { return version < "0.10.0"_v; }
    bool defi1() { return version >= "0.10.0"_v; }
};

inline ProtocolVersion NodeVersion::protocol()
{
    return { *this };
}
