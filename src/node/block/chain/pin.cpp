#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("3389f6dd0b5fb91a364efc8945da87ff3e8d721b38353731cdb60800000000002a8ea2d11a75d2ee2ff0f3cfa9bbf491c287616205568c5ee24d4741e818aff801f6bf930000000164e2f2c23b682077");
    return GridPin{Batchslot(26),h};
}
