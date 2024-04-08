#include "block/chain/state.hpp"
#include "communication/messages.hpp"

bool Descripted::valid()
{
    return ((_chainLength == 0) == (_worksum.is_zero())
        && _grid.slot_end().upper() > _chainLength
        && _grid.slot_end().offset() <= _chainLength);
}

Descripted::Descripted(Descriptor descriptor, Height chainLength, Worksum worksum, HashGrid grid)
        : descriptor(descriptor)
        , _chainLength(chainLength)
        , _worksum(worksum)
        , _grid(grid) {
            if (!valid()) {
                throw Error(EINVDSC);
            }
        }
void Descripted::append_throw(const AppendMsg& msg)
{
    if (worksum() >= msg.worksum || chain_length() >= msg.newLength) {
        throw Error(EAPPEND);
    }
    _chainLength = msg.newLength;
    _worksum = msg.worksum;
    _grid.append(msg.grid);
    if (!valid()) {
        throw Error(EAPPEND);
    }
}

void Descripted::deprecate()
{
    deprecation_time = std::chrono::steady_clock::now();
}
bool Descripted::expired() const
{
    using namespace std::chrono;
    if (!deprecation_time)
        return false;
    return *deprecation_time + minutes(8) < steady_clock::now();
}
