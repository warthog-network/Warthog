#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("f85faa8f856c62ddc5ef001a946d2f4be3fd866b83db169b4b1051803ec700ed0b6647a85d6a66c05825724a546ec685b4899ef10ecf2a5f3e04942c48f9daf0017b7656000000026585149c145f8054");
    return GridPin{Batchslot(87),h};
}
