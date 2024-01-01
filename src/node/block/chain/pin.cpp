#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("ca4492cb844ece68052cb77d05026fd0becc8aba3d02a3775e5f9147f9311da40a7657df07d1b69662b68e30f5755b5c5d45d04c7ebb759f1cdee0b6ce58ede0df735b01000000026592179640636cdd");
    return GridPin{Batchslot(92),h};
}
