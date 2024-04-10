#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("e08d066d686d5b264db7007286a3be93fd3853b9f9b9bbe03afbc2ff9f07b33d0afd15fa022ca40e560d6a7925659b7e3550246bd258c63b7ff17fbc343aef02bcc480c50000000265cc1c15c8e352fe");
    return GridPin{Batchslot(114),h};
}
