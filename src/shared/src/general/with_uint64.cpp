#include "with_uint64.hpp"
#include "nlohmann/json.hpp"
#include "reader.hpp"

IsUint32::IsUint32(Reader& r)
    : val(r.uint32()) {};

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
