#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("46d35ed8925c8352ba39724f6300d722aa68647d384a1337cc1e2c0a0000000021cbd7eeb4de9855cf012681cc0136970ec2d0908f67e27e67a776b73938fd5bc028991e0000000164acb408c64731c8");
    return GridPin{Batchslot(5),h};
};
