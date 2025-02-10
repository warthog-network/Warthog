#include "past_chains.hpp"
#include "consensus.hpp"
#include "chainserver/db/chain_db.hpp"

namespace chainserver {
using namespace std::chrono;
void BlockCache::schedule(std::variant<DiscardedStageSchedule, ChainSchedule> v,DeletionKey dk)
{
    auto discard_at = system_clock::now() + minutes(10);
    assert(gcSchedule.try_emplace(discard_at, 
            DeleteScheduleEntry {
                        .data { std::move(v) },
                        .deletionKey { dk } }).second);
}

std::shared_ptr<Headerchain> BlockCache::add_old_chain(const Chainstate& consensus, DeletionKey dk)
{
    auto headers_ptr = std::make_shared<Headerchain>(*static_cast<const Headerchain*>(&consensus.headers()));
    std::unique_lock<std::mutex> lchains(mutex);
    auto [iter, inserted] = chains.try_emplace(consensus.descriptor(), headers_ptr);
    assert(inserted);
    schedule( ChainSchedule { iter },dk); 
    return headers_ptr;
}

void BlockCache::schedule_discard(DeletionKey dk) {
    schedule(DiscardedStageSchedule{},dk);
}

Batch BlockCache::get_batch(const BatchSelector& s) const
{
    std::unique_lock<std::mutex> lchains(mutex);
    auto iter = chains.find(s.descriptor);
    if (iter == chains.end())
        return {};
    return iter->second.headers->get_headers(s.header_range());
}
std::optional<HeaderView> BlockCache::get_header(Descriptor descriptor, Height height) const
{
    std::unique_lock<std::mutex> lchains(mutex);
    auto iter = chains.find(descriptor);
    if (iter == chains.end())
        return {};
    return iter->second.headers->get_header(height);
}

void BlockCache::handle(DiscardedStageSchedule) {

}

void BlockCache::handle(ChainSchedule cs)
{
    chains.erase(cs.iter);
}

void BlockCache::garbage_collect(ChainDB& db)
{
    std::unique_lock l(mutex);
    if (chains.size() == 0)
        return;

    auto now = std::chrono::system_clock::now();
    for (auto iter { gcSchedule.begin() }; iter != gcSchedule.end();) {
        auto& [t, entry] { *iter };
        if (t > now)
            break;
        db.garbage_collect_blocks(entry.deletionKey);
        std::visit([&](auto& data) {
            handle(data);
        },
            entry.data);
        gcSchedule.erase(iter++);
    }
}
std::vector<Hash> BlockCache::get_hashes(const DescriptedBlockRange& r) const
{
    std::unique_lock<std::mutex> lchains(mutex);
    auto iter = chains.find(r.descriptor);
    if (iter == chains.end())
        return {};
    auto& chain = iter->second.headers;
    if (chain->length() < r.last())
        return {};
    std::vector<Hash> hashes(r.length());
    for (NonzeroHeight h = r.first(); h < r.last() + 1; ++h) {
        hashes[h - r.first()] = chain->hash_at(h);
    }
    return hashes;
}
}
