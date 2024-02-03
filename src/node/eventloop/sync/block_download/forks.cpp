#include "forks.hpp"
#include "block/chain/state.hpp"
#include "eventloop/types/conref_impl.hpp"

namespace BlockDownload {

// void Forks::update_fork_iter(Conref c)
// {
//     auto& d { data(c) };
//     if (d.forkIter != forks.end())
//         forks.erase(d.forkIter);
//     assert(d.forkRange.lower() <= d._descripted->chain_length() + 1);
//     d.forkIter = forks.emplace(d.forkRange.lower(), c);
//     assert(d.forkIter->first == d.forkRange.lower());
// }

void Forks::pin_current_chain(Conref c)
{
    auto pin { c->chain.descripted() };
    auto& d = data(c);
    erase(c);
    auto fr { c->chain.stage_fork_range() };
    auto iter = forks.emplace(fr.lower(), c);
    d.forkData = ConnectionData::ForkData(pin, iter, fr);
}

void Forks::pin_leader_chain(Conref c, std::shared_ptr<Descripted> pin, ForkRange fr)
{
    auto& d { data(c) };
    assert(!d.forkData.has_value());
    auto iter = forks.emplace(fr.lower(), c);
    d.forkData = ConnectionData::ForkData(pin, iter, fr);
}


void Forks::replace_fork_iter(ConnectionData::ForkData& fd, Conref c, ForkRange fr)
{
    forks.erase(fd.iter());
    auto iter = forks.emplace(fr.lower(), c);
    fd.udpate_iter_range(iter, fr);
};

std::optional<Height> Forks::reachable_length() const
{
    if (forks.size() == 0)
        return {};
    return forks.rbegin()->first - 1;
}

void Forks::match(Conref c, const Headerchain& headers, NonzeroHeight h, HeaderView hv)
{
    auto& d { data(c) };
    assert(d.forkData.has_value());
    auto& fd { d.forkData.value() };
    auto fr { fd.range() };

    if (fr.match(headers, h, hv).changedLower)
        replace_fork_iter(fd, c, fr);
};

void Forks::clear()
{
    for (auto& [h, c] : forks) {
        auto& fd { data(c) };
        fd.forkData.reset();
    }
    forks.clear();
}

void Forks::erase(Conref c)
{
    auto& d { data(c) };
    if (d.forkData) {
        forks.erase(d.forkData->iter());
        d.forkData.reset();
    }
}

}
