#pragma once
#include "version_def.hpp"
#include <cstdint>
#include <string>

class ProtocolVersion;
class NodeVersion {
    constexpr NodeVersion(uint32_t v)
        : version(v) { };

public:
    constexpr NodeVersion()
    {
    }
    constexpr NodeVersion(uint8_t major, uint8_t minor, uint8_t patch)
        : version((uint32_t(major) << 16) | (uint32_t(minor) << 8) | (uint32_t(patch)))
    {
    }
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

class ProtocolVersion : protected NodeVersion {
    friend class NodeVersion;

private:
    ProtocolVersion(NodeVersion nv)
        : NodeVersion(nv) { };

public:
    bool v1() { return version < 0x500 || ((version >> 8) == 6); /*0.5.00 */ }
    bool v2() { return !v1() && (version <= ((7 << 8) + 54)); /*<=0.7.54 */ }
    bool v3() { return version > ((7 << 8) + 54); /*>0.7.54 */ }
};

inline ProtocolVersion NodeVersion::protocol()
{
    return { *this };
}
