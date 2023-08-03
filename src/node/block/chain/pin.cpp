#include "pin.hpp"
#include "block/header/header.hpp"

std::optional<GridPin> GridPin::checkpoint(){
    // UPDATE GRID PIN HERE (HARD-CODED CHAIN CHECKPOINT)
    static Header h("11a7f25db010104babe137b0be474d0c9a1ff5364d86aed8f2f6af040000000025d405c6ceee3fe8e1eab9414d20f7a29465c14bcc5445a890616f4beb783a226487606f0000000164c937ca3199c8ec");
    return GridPin{Batchslot(16),h};
};
