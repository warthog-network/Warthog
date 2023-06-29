#include "header.hpp"
#include "general/hex.hpp"

Header::Header(const char* v)
    :Header(hex_to_arr<80>(v))
{}
