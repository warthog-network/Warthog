#include "errors.hpp"
#include "block/chain/height.hpp"
#include <uv.h>

namespace errors {
#define ERR_NAME_GEN(code, name, _) \
    case code:                      \
        return #name;
const char* err_name(int err)
{

    switch (err) {
        ADDITIONAL_ERRNO_MAP(ERR_NAME_GEN)
    }
    return uv_err_name(err);
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
    return uv_strerror(err);
}
#undef STRERROR_GEN

} // namespace errors

ChainError::ChainError(int32_t e, NonzeroHeight height)
    : Error(e)
    , h(height.value()) {};

NonzeroHeight ChainError::height() const
{
    return NonzeroHeight(h);
};
