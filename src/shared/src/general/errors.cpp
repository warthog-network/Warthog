#include "errors.hpp"
#include "block/chain/height.hpp"
#ifndef DISABLE_LIBUV
#include <uv.h>
#endif

#define STRERROR_GEN(code, name, str) \
    case code:                        \
        return str;
const char* Error::strerror() const
{
    switch (code) {
        ADDITIONAL_ERRNO_MAP(STRERROR_GEN)
    }
#ifndef DISABLE_LIBUV
    return uv_strerror(code);
#endif
    return "unknown error";
}
#undef STRERROR_GEN

#define ERR_NAME_GEN(code, name, _) \
    case code:                      \
        return #name;
const char* Error::err_name() const
{

    switch (code) {
        ADDITIONAL_ERRNO_MAP(ERR_NAME_GEN)
    }
#ifndef DISABLE_LIBUV
    return uv_err_name(code);
#endif
    return "unknown";
#undef ERR_NAME_GEN
}

std::string Error::format() const { return std::string(err_name()) + " (" + strerror() + ")"; }

bool Error::triggers_ban() const
{
    if (code <= 0 || code >= 200)
        return false;
    switch (code) {
    case ECHECKSUM:
        //
        // We are not sure the following are triggered by evil behavior or bug.
        // Let's observe for some more time before enable banning on them.
    case EEMPTY:
    case EPROBEDESCRIPTOR:
        return false;
    default:
        return true;
    }
    return code != ECHECKSUM;
}

uint32_t Error::bantime() const { 
    if (triggers_ban())
        return 20 * 60; /* 20 minutes*/ 
    return 0;
}

ChainError::ChainError(Error e, NonzeroHeight height)
    : Error(e)
    , h(height.value()) {};

NonzeroHeight ChainError::height() const
{
    return NonzeroHeight(h);
};
