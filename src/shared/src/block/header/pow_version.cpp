#include "pow_version.hpp"
std::optional<POWVersion> POWVersion::from_params(NonzeroHeight height, uint32_t version, bool testnet)
{
    if (testnet) {
        if (height.value() <= 2) {
            return POWVersion { Janus7 {} };
        } else {
            return POWVersion { Janus8 {} };
        }
    } else { // main net
        if (height.value() <= JANUSV1RETARGETSTART) {
            return POWVersion { Original {} };
        } else { // height > JANUSV1RETARGETSTART
            if (height.value() > JANUSV8BLOCKV3START) {
                if (version != 3)
                    return {};
                return POWVersion { Janus8 {} };
            } else { // height <= JANUSV8RETARGETSTART
                if (version != 2)
                    return {};
                if (height.value() > JANUSV8BLOCKV3START) {
                    return {};
                }
                if (height.value() > JANUSV7RETARGETSTART)
                    return POWVersion { Janus7 {} };
                if (height.value() > JANUSV6RETARGETSTART)
                    return POWVersion { Janus6 {} };
                if (height.value() > JANUSV5RETARGETSTART)
                    return POWVersion { Janus5 {} };
                if (height.value() > JANUSV4RETARGETSTART)
                    return POWVersion { Janus4 {} };
                if (height.value() > JANUSV3RETARGETSTART)
                    return POWVersion { Janus3 {} };
                if (height.value() > JANUSV2RETARGETSTART)
                    return POWVersion { Janus2 {} };
                assert(height.value() > JANUSV1RETARGETSTART);
                return POWVersion { Janus1 {} };
            }
        }
    }
}
