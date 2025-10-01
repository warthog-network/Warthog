#include "height.hpp"
#include "general/errors.hpp"
#include "general/writer.hpp"

Writer& operator<<(Writer& w, Height h)
{
    return w << h.value();
};

NonzeroHeight::NonzeroHeight(Reader& r)
    : IsUint32(Height(r).nonzero_throw(EBADHEIGHT).value()) {};
