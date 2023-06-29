#pragma once
#include "block/chain/header_chain.hpp"
#include "db/chain/deletion_key.hpp"
class ChainDB;

namespace chainserver {
class BlockCache {
public:
    [[nodiscard]] std::shared_ptr<Headerchain> add_old_chain(const Chainstate&, DeletionKey); //OK
    void schedule_discard(DeletionKey); 
    Batch get_batch(const BatchSelector& s) const;
    std::optional<HeaderView> get_header(Descriptor descriptor, Height height) const;
    void garbage_collect(ChainDB&);
    std::vector<Hash> get_hashes(const DescriptedBlockRange&) const;

private:
    struct Entry {
        std::shared_ptr<Headerchain> headers;
        Entry(std::shared_ptr<Headerchain> headers)
            : headers(std::move(headers)){};
    };
    mutable std::mutex mutex;
    std::map<Descriptor, Entry> chains; // by chain id
                                      //

    struct DiscardedStageSchedule {
    };
    struct ChainSchedule {
        decltype(chains)::iterator iter;
    };

    void handle(DiscardedStageSchedule);
    void handle(ChainSchedule);
    struct DeleteScheduleEntry {
        std::variant<DiscardedStageSchedule, ChainSchedule> data;
        DeletionKey deletionKey;
    };
    void schedule(std::variant<DiscardedStageSchedule, ChainSchedule>, DeletionKey);

    using tp = std::chrono::system_clock::time_point;
    std::map<tp, DeleteScheduleEntry> gcSchedule;
};
};
