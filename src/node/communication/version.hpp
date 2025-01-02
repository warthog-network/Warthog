#pragma once
#include <string>
#include <cstdint>

class ProtocolVersion;
class NodeVersion {
public:
    friend class ProtocolVersion;
    NodeVersion(uint32_t v)
        : version(v) {};
    uint32_t raw() const { return version; }
    std::string to_string() const;
    ProtocolVersion protocol();
    uint8_t major() const { return 0xFF & (version >> 16); }
    uint8_t minor() const { return 0xFF & (version >> 8); }
    uint8_t patch() const { return 0xFF & version; }

private:
    uint32_t version;
};

class ProtocolVersion : protected NodeVersion {
    friend class NodeVersion;

private:
    ProtocolVersion(NodeVersion nv)
        : NodeVersion(nv) {};

public:
    bool v1() { return version < 0x500 || ((version >> 8) == 6); /*0.5.00 */ }
    bool v2() { return !v1() && (version <= ((7 << 8) + 54)); /*<=0.7.54 */ }
    bool v3() { return version > ((7 << 8) + 54); /*>0.7.54 */ }
};

inline ProtocolVersion NodeVersion::protocol()
{
    return { *this };
}
