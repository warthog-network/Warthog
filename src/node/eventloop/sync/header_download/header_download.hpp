#pragma once
#include "../request_sender_declaration.hpp"
#include "block/chain/consensus_headers.hpp"
#include "block/chain/offender.hpp"
#include "eventloop/sync/header_download/rogue_headerheights.hpp"
#include "eventloop/types/conndata.hpp"
#include "eventloop/types/peer_requests.hpp"
#include <deque>
#include <set>

struct Inspector;
namespace HeaderDownload {

struct LeaderInfo {
    Conref cr;
    std::shared_ptr<Descripted> descripted;
};

struct VerifierNode;
using VerifierMap = std::map<Header, VerifierNode, HeaderView::HeaderComparator>;
using Ver_iter = VerifierMap::iterator;

struct ReqData;
struct RequestNode {
    std::optional<Conref> cr;
    uint64_t originId;
    Batch batch;

    // methods
    bool filled() const { return batch.size() > 0; }
    bool pending() const { return cr.has_value(); }
};

struct LeaderNode;
using Lead_list = std::list<LeaderNode>;
using Lead_iter = Lead_list::iterator;
using Lead_set = std::set<Lead_iter>;

inline bool operator<(const Lead_iter& l1, const Lead_iter& l2)
{
    return &*l1 < &*l2;
}

struct NonzeroSnapshot {
    NonzeroSnapshot(std::shared_ptr<Descripted>);
    std::shared_ptr<Descripted> descripted;
    NonzeroHeight length;
    Worksum worksum;
};
struct VerifierNode {
    VerifierNode(SharedBatch&& b);
    VerifierNode(SharedBatch&& b, HeaderVerifier&& hv);
    size_t refcount = 0;
    HeaderVerifier verifier;
    // Lead_vec leaders;
    SharedBatch sb;
};
inline bool operator<(const Ver_iter& l1, const Ver_iter& l2)
{
    return &*l1 < &*l2;
}
struct QueueBatchNode {
    std::optional<Conref> cr;
    uint64_t originId;
    Batch batch;
    Lead_set leaderRefs;
    std::vector<Conref> probeRefs;
    bool has_pending_request() { return cr.has_value(); }
};
using Queued_iter = std::map<Header, QueueBatchNode>::iterator;
inline bool operator<(const Queued_iter& l1, const Queued_iter& l2)
{
    return &*l1 < &*l2;
}

struct QueueEntry {
    std::optional<Header> prevHeader;
    Queued_iter iter;
};

struct LeaderNode {
public:
    LeaderNode(Conref cr, NonzeroSnapshot&& snapshot, ProbeData&& pd, VerifierMap::iterator verifier)
        : cr(cr)
        , snapshot(std::move(snapshot))
        , verifier { verifier }
        , probeData(std::move(pd))
    {
    }
    LeaderNode(Conref cr, NonzeroSnapshot&& snapshot, ProbeData&& pd) // with no verifier reference (starts from genesis)
        : cr(cr)
        , snapshot(std::move(snapshot))
        , probeData(std::move(pd))
    {
    }
    const Conref cr;
    const NonzeroSnapshot snapshot;

    struct {
        Batch batch;
        Worksum claimedWork;
    } finalBatch;
    Batchslot final_slot()
    {
        return Batchslot(snapshot.length);
    }

    std::deque<QueueEntry> queuedIters; // OK
    std::optional<Ver_iter> verifier; // without value if from genesis
    ProbeData probeData;
    Batchslot next_slot();
    Worksum verified_total_work();
    Worksum final_batch_work();

private:
    struct Queued {
        LeaderNode& ln;
        struct iterator {
            size_t i = 0;
            Queued& q;
            iterator& operator++()
            {
                i += 1;
                return *this;
            }
            bool is_solo() const
            {
                if (q.ln.queuedIters.size() != 1)
                    return false;
                assert(i == 0);
                return true;
            }

            std::optional<ChainPin> pin_prev() const
            {
                auto& entry = q.ln.queuedIters[i];
                if (entry.prevHeader) {
                    return ChainPin { slot().offset(), *entry.prevHeader };
                }
                return {};
            }

            operator ReqData() const;

            auto& operator*() { return *this; }
            auto& node() { return q.ln.queuedIters[i].iter->second; }
            Batchslot slot() const { return q.ln.next_slot() + i; }
        };
        struct sentinel {
        };

        iterator begin()
        {
            return { 0, *this };
        }
        sentinel end() { return {}; }
        friend bool operator==(const iterator& iter, const sentinel&)
        {
            return iter.i >= iter.q.ln.queuedIters.size();
        }
    };

public:
    Queued queued()
    {
        return { *this };
    }
};

class Downloader {
    friend struct ::Inspector;

    struct Offender {
        ChainOffender co;
        Conref internal;
        Offender(ChainError ce, Conref cr)
            : co(ce, cr.id())
            , internal(cr) { };
    };

public:
private:
    auto& data(Conref cr) { return cr->usage.data_headerdownload; }
    auto& data(Conref cr) const { return cr->usage.data_headerdownload; }
    //
    // probe link functions
    void clear_connection_probe(Conref cr, bool eraseFromContainer);
    void set_connection_probe(Conref cr, ProbeData&& d, std::shared_ptr<Descripted>, Queued_iter iter);
    void clear_leader_probe(Lead_iter);
    //
    // leader functions
    void prune_leaders();
    bool is_leader(Conref cr) const
    {
        return data(cr).leaderIter != leaderList.end();
    }

    struct ConnectionFinder {
        ConnectionFinder(RequestSender& s, std::vector<Conref>& v)
            : s(s)
            , connections(v)
        {
        }
        RequestSender& s;
        const std::vector<Conref>& connections;
        size_t conIndex = 0;
    };

    void find(HeaderView hv);
    std::pair<Lead_set, Lead_set> split_leaders(const Lead_set& leaders, HeaderView h, size_t batchIndex);

public:
    std::vector<ChainOffender> on_rogue_header(const RogueHeaderData& rogueHeaderData);
    size_t size() const { return connections.size(); }
    [[nodiscard]] bool is_active() const { return !leaderList.empty(); }
    Downloader(const StageAndConsensus& cache, Worksum minWork)
        : chains(cache)
        , minWork(minWork)
    {
    }
    void set_min_worksum(const Worksum& ws);

    // peer message callbacks
    void on_append(Conref cr);
    void on_fork(Conref cr);
    void on_rollback(Conref cr);

    void on_signed_snapshot_update();
    void insert(Conref cr);
    bool erase(Conref cr);

    auto leaders_end() { return leaderList.end(); }

    [[nodiscard]] std::vector<ChainOffender> do_header_requests(RequestSender);
    void do_probe_requests(RequestSender);

    void on_request_expire(Conref cr, const HeaderRequest& msg);
    void on_proberep(Conref c, const Proberequest& req, const ProberepMsg&);
    void on_probe_request_expire(Conref cr);
    [[nodiscard]] std::vector<ChainOffender> on_response(Conref cr, HeaderRequest&&, Batch&&);
    [[nodiscard]] std::optional<std::tuple<LeaderInfo, Headerchain>> pop_data();

private:
    bool do_exclusive_final_requests(RequestSender&);
    std::vector<ChainOffender> do_shared_grid_requests(RequestSender&);

    bool has_data() const;
    void select_leaders();

    void process_final(Lead_iter, std::vector<Offender>& out);

    std::optional<Conref> try_send(ConnectionFinder& cf, std::vector<ChainOffender> close, const ReqData&);
    bool try_final_request(Lead_iter, RequestSender& s);

    std::vector<ChainOffender> filter_leadermismatch_offenders(std::vector<Offender>);

    // verifier related
    bool advance_verifier(const Ver_iter* vi, const Lead_set&, const Batch& b,
        std::vector<Offender>& out);
    Ver_iter acquire_verifier(SharedBatch&&);
    void release_verifier(Ver_iter);
    void verify_queued(Queued_iter qi, const Lead_set& leaders, std::vector<Offender>& out);

    // queued batch related
    std::map<Header, QueueBatchNode> queuedBatches;
    void acquire_queued_batch(std::optional<Header>, HeaderView, Lead_iter);
    void release_first_queued_batch(Lead_iter);
    void shift_queued_batch(Lead_iter);

    // leader related functions
    void erase_leader(Lead_iter);
    void queue_requests(Lead_iter);
    bool consider_insert_leader(Conref cr); // returns true if has effect
    bool can_insert_leader(Conref cr);

    bool valid_shared_batch(const SharedBatch&);
    bool has_header(LeaderNode&, HeightHeader);
    bool contains_rogue_blocks(const HeaderchainSkeleton&);
    bool flag_connection(Conref);

private: // data
    VerifierMap verifierMap;
    struct Maximizer {
        LeaderInfo leaderInfo;
        HeaderchainSkeleton headers;
        Worksum worksum;
    };
    std::optional<Maximizer> maximizer;
    size_t pendingDepth = 10;
    size_t maxLeaders = 10;
    RogueHeaders rogueHeaders;

    Lead_list leaderList;
    std::vector<Conref> connections;
    std::vector<Conref> connectionsWithProbeJob;
    const StageAndConsensus& chains;
    Worksum minWork;
};
}
