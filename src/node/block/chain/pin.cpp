#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("7aff2664ca3a0af50052911067b113920811658d954dfcae56f390010000000025aa4d59d154af55ccea208b574033fc70eba69e925c87809470f0aa88e105c7bf4f9d130000000164c68d946e7c5e01");
    return GridPin{Batchslot(15),h};
};
