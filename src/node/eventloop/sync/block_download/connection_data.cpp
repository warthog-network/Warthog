#include "connection_data.hpp"
#include "block/chain/state.hpp"
#include "eventloop/types/conref_declaration.hpp"
#include "eventloop/types/conref_impl.hpp"
#include "focus.hpp"
class Focus;
namespace BlockDownload {

[[nodiscard]] ConnectionData& data(Conref cr)
{
    return cr->usage.data_blockdownload;
};

ConnectionData::ForkData::IterRange::IterRange(ForkIter iter, ForkRange r)
    : _iter(iter)
    , _range(std::move(r))
{
    assert(iter->first == _range.lower());
}

ConnectionData::ForkData::ForkData(std::shared_ptr<Descripted> d, ForkIter fi, ForkRange forkRange)
    : _descripted(std::move(d))
    , iterRange { fi, forkRange }
{
    assert(iterRange._range.lower() <= _descripted->chain_length() + 1);
}

void ConnectionData::ForkData::ForkData::udpate_iter_range(ForkIter iter, ForkRange range)
{
    iterRange = { iter, range };
    assert(iterRange._range.lower() <= _descripted->chain_length() + 1);
}

}
