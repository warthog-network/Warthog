#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("1b852e408eee8c0a20f8efd9ddc45248a531cdc61b08a75900ff1200000000002bf2f2acf8d2ff7732759b81c960747c8047cc57346e9b24d9bed711dc75890d14f945f00000000165754d298d0a9611");
    return GridPin{Batchslot(81),h};
}
