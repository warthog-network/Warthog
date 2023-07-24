#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("cd89a131714165890c649e6a2c25631c60465b9f297faaf172e0c3010000000026af9f00b79725186be4c29433d28e00fcc5b2cc99173b6deb66c664663b570e5748aa9f0000000164be757e890545c5");
    return GridPin{Batchslot(12),h};
};
