#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("73ee0bdf9f0053ee0daad4a76a86481c284ca9397eb0b57a1c322600000000002adb56f5bd574a0c7fd6b0b7954630869f90c11519a598d630d389a57e780448ad2706dc0a0000016537938a15587c5d");
    return GridPin{Batchslot(58),h};
}
