#include "forks.hpp"
#include "block/chain/state.hpp"
#include "eventloop/types/conref_impl.hpp"

namespace BlockDownload {

void Forks::update_fork_iter(Conref c)
{
    auto& d { data(c) };
    if (d.forkIter != forks.end())
        forks.erase(d.forkIter);
    assert(d.forkRange.lower() <= d._descripted->chain_length() + 1);
    d.forkIter = forks.emplace(d.forkRange.lower(), c);
    assert(d.forkIter->first == d.forkRange.lower());
}

void Forks::link(Conref c)
{

    auto& d = data(c);
    d.forkRange = c->chain.stage_fork_range();
    d._descripted = c->chain.descripted();
    assert(d.forkRange.lower() <= d._descripted->chain_length() + 1);
    update_fork_iter(c);
    assert(d.fork_iter()->first == d.forkRange.lower());
}
std::optional<Height> Forks::reachable_length() const
{
    if (forks.size() == 0)
        return {};
    return forks.rbegin()->first - 1;
}

void Forks::match(Conref c, const Headerchain& headers, NonzeroHeight h, HeaderView hv)
{
    auto& d { data(c) };
    if (d.forkRange.match(headers, h, hv).changedLower)
        update_fork_iter(c);
    assert(d.forkRange.lower() <= d._descripted->chain_length() + 1);
    assert(d.forkIter->first == d.forkRange.lower());
};


void Forks::assign(Conref c, std::shared_ptr<Descripted> descripted, ForkRange fr)
{
    auto& d { data(c) };
    d._descripted = std::move(descripted);
    d.forkRange = fr;
    update_fork_iter(c);
    assert(d.forkRange.lower() <= d._descripted->chain_length() + 1);
    assert(d.forkIter->first == d.forkRange.lower());
}

void Forks::clear()
{
    for (auto& [h, c] : forks) {
        auto& fd { data(c) };
        fd.forkIter = forks.end();
        fd._descripted = {};
        fd.forkRange = {};
    }
    forks.clear();
}

void Forks::erase(Conref c)
{
    auto& fd { data(c) };
    if (fd.forkIter == forks.end())
        return;
    forks.erase(fd.forkIter);
    fd.forkIter = forks.end();
    fd._descripted = {};
    fd.forkRange = {};
};

}
