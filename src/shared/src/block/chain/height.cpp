#include "height.hpp"
#include "general/errors.hpp"
Writer& operator<<(Writer& w, Height h)
{
    return w << h.value();
};
PinHeight::PinHeight(const Height h)
    : Height(h)
{
    if (!h.is_pin_height())
        throw Error(ENOPINHEIGHT);
};

NonzeroHeight::NonzeroHeight(Reader& r)
    : IsUint32(Height(r).nonzero_throw(EBADHEIGHT).value()) {};
