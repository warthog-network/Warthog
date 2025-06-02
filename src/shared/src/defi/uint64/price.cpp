
#include "price.hpp"
#include "general/reader.hpp"
Price_uint64::Price_uint64(Reader& r)
    : Price_uint64(from_uint32_throw(r.uint32()))
{
}
