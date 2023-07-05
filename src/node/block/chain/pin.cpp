#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("2e92a976c5f7df4cfa26a37f534f79795d23342cd003fdaa000d28050000000021cab3fb524f9a8ca2cc60fbe97602c87079628007cedffdc35cfe9c2ee213fd065bfd890000000164a4c5ec4b03db81");
    return GridPin{Batchslot(2),h};
};
