#pragma once
#include <cstdint>

class ProtocolVersion {
public:
    ProtocolVersion(uint32_t v)
        : version(v) {};
    uint32_t raw() const { return version; }
    bool v1() { return version < 0x500; /*0.5.00 */ }
    bool v2() { return version >= 0x500; /*0.5.00 */ }

private:
    uint32_t version;
};
