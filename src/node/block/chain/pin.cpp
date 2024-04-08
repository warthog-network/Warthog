#include "pin.hpp"
#include "block/header/header.hpp"
#include "general/hex.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Hash h(hex_to_arr<32>("11e188857b9c78a313cb038fd2f1f8d9c7ced89342b0f525f5dff5e2259c0101"));
    return GridPin{Batchslot(141),h};
}
