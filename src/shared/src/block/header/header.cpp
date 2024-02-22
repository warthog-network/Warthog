#include "header.hpp"
#include "general/hex.hpp"

Header::Header(const char* v)
    : Header(hex_to_arr<80>(v))
{
}


void Header::set_timestamp(std::array<uint8_t, 4> arr)
{
    std::copy(arr.begin(), arr.end(), begin() + HeaderView::offset_timestamp);
}
void Header::set_nonce(std::array<uint8_t, 4> arr)
{
    std::copy(arr.begin(), arr.end(), begin() + HeaderView::offset_nonce);
}
