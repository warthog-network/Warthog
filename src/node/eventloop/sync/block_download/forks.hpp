#pragma once
#include "eventloop/sync/block_download/connection_data.hpp"
#include "eventloop/types/conref_declaration.hpp"
namespace BlockDownload {

class Forks {
public:
    size_t size() const { return forks.size(); }
    void pin_current_chain(Conref c);
    void pin_leader_chain(Conref, std::shared_ptr<Descripted>, ForkRange);
    std::optional<Height> reachable_length() const;
    void match(Conref c, const Headerchain& hc, NonzeroHeight h, HeaderView hv);
    void clear();
    auto lower_bound(NonzeroHeight h)
    {
        return forks.lower_bound(h);
    }
    auto end() { return forks.end(); }
    void erase(Conref);
    auto rbegin() { return forks.rbegin(); }

private:
    void reset(Conref c);
    void replace_fork_iter(ConnectionData::ForkData& fd,Conref c, ForkRange fr);
    auto& forkdata(Conref c);
    Forkmap forks;
};
}
