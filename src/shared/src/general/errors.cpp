#include "errors.hpp"
#include "block/chain/height.hpp"
#ifndef DISABLE_LIBUV
#include <uv.h>
#endif

namespace errors {
#define ERR_NAME_GEN(code, name, _) \
    case code:                      \
        return #name;
const char* err_name(int err)
{

    switch (err) {
        ADDITIONAL_ERRNO_MAP(ERR_NAME_GEN)
    }
#ifndef DISABLE_LIBUV
    return uv_err_name(err);
#endif
    return "unknown";
}
#undef ERR_NAME_GEN

#define STRERROR_GEN(code, name, str) \
    case code:                        \
        return str;
const char* strerror(int err)
{
    switch (err) {
        ADDITIONAL_ERRNO_MAP(STRERROR_GEN)
    }
#ifndef DISABLE_LIBUV
    return uv_strerror(err);
#endif
    return "unknown error";
}
#undef STRERROR_GEN

} // namespace errors

ChainError::ChainError(Error e, NonzeroHeight height)
    : Error(e)
    , h(height.value()) {};

NonzeroHeight ChainError::height() const
{
    return NonzeroHeight(h);
};
