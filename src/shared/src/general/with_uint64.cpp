#include "with_uint64.hpp"
#include "nlohmann/json.hpp"
#include "reader.hpp"
#include <limits>

IsUint32::IsUint32(Reader& r)
    : val(r.uint32()) {};
IsUint32::IsUint32(int64_t w)
    : val(w)
{
    assert(w >= 0);
    assert(w <= std::numeric_limits<uint32_t>::max());
};
IsUint32::IsUint32(size_t w)
    : val(w)
{
    assert(w <= std::numeric_limits<uint32_t>::max());
};

IsUint64::IsUint64(Reader& r)
    : val(r.uint64()) {};

IsUint32::operator nlohmann::json() const
{
    return nlohmann::json(val);
};
IsUint64::operator nlohmann::json() const
{
    return nlohmann::json(val);
};
