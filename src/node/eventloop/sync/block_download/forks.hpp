#pragma once
#include "eventloop/sync/block_download/connection_data.hpp"
#include "eventloop/types/conref_declaration.hpp"
namespace BlockDownload {

class Forks {
public:
    size_t size() const { return forks.size(); }
    void link(Conref c);
    std::optional<Height> reachable_length() const;
    void match(Conref c, const Headerchain& hc, NonzeroHeight h, HeaderView hv);
    void assign(Conref, std::shared_ptr<Descripted>, ForkRange);
    void clear();
    auto lower_bound(NonzeroHeight h)
    {
        return forks.lower_bound(h);
    }
    auto end() { return forks.end(); }
    void erase(Conref);
    auto rbegin() { return forks.rbegin(); }

private:
    void update_fork_iter(Conref c);
    auto& forkdata(Conref c);
    Forkmap forks;
};
}
