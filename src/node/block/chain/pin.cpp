#include "pin.hpp"
#include "block/header/header.hpp"

wrt::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("34b4064fced07341204e3b2283477d5fc6423d7f8244f83d6e1fa1551a74ad470ba7c02fc4c7d16b758e8af266349dc1070727b5ecd5d7fc6c6417d2458018e328818355000000036748585daf74da6b");
    return GridPin{Batchslot(259),h};
}
