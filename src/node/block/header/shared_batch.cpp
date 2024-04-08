#include "block/chain/consensus_headers.hpp"

SharedBatch::~SharedBatch()
{
    if (!valid())
        return;
    Nodedata& nd = data.iter->second;
    nd.registry.dec_ref(data.iter);
}
SharedBatch::SharedBatch(const SharedBatch& other)
    : data({ .iter = other.data.iter })
{
    if (!valid())
        return;
    Nodedata& nd = data.iter->second;
    std::unique_lock l(nd.registry.m);
    nd.refcount += 1;
}

SharedBatch::SharedBatch(const SharedBatchView& v) noexcept
    : data({ .iter = static_cast<iter_type>(v.data.iter) })
{
    if (!valid())
        return;
    Nodedata& nd = data.iter->second;
    std::unique_lock l(nd.registry.m);
    nd.refcount += 1;
}

SharedBatch::SharedBatch(SharedBatch&& other) noexcept
    : data({ .iter = other.data.iter })
{
    other.data.raw = 0;
}

SharedBatch& SharedBatch::operator=(const SharedBatch& other)
{
    return (*this = SharedBatch(other));
}

SharedBatch& SharedBatch::operator=(const SharedBatchView& other)
{
    return (*this = SharedBatch(other));
}

SharedBatch& SharedBatch::operator=(SharedBatch&& other)
{
    if (valid()) {
        Nodedata& nd = data.iter->second;
        nd.registry.dec_ref(data.iter);
    }
    data.iter = other.data.iter;
    other.data.raw = 0;
    return *this;
}

SharedBatch::SharedBatch(iter_type iter) noexcept
    : data({ .iter = iter })
{
    // no lock here!!!
    if (!valid())
        return;
    Nodedata& nd = data.iter->second;
    nd.refcount += 1;
}

HeaderVerifier SharedBatch::verifier() const
{
    return *this;
}

SharedBatch BatchRegistry::share(Batch&& headerbatch, HashView hash, const SharedBatch& prev)
{
    assert(headerbatch.complete());
    Worksum totalWork = prev.total_work() + headerbatch.worksum(prev.upper_height());
    return share(std::move(headerbatch), hash, prev, totalWork);
}

SharedBatch BatchRegistry::share(Batch&& headerbatch, HashView hash, const SharedBatch& prev, Worksum totalWork) // TODO: provide key to avoid recomputation by hashing
{
    assert(headerbatch.complete());
    std::unique_lock l(m);
    auto iter = headers.find(hash);
    if (iter == headers.end()) {
        // check prevalid
        return headers.try_emplace(
                          hash,
                          *this, std::move(headerbatch), totalWork, SharedBatch(prev.data.iter))
            .first;
    } else {
        assert(headerbatch == iter->second.batch);
        assert(totalWork == iter->second.totalWork);
        return SharedBatch(iter);
    }
}

template <typename T>
SharedBatchView BatchRegistry::find_last_template(const T& t)
{
    Batchslot a(0);
    Batchslot b(t.slot_end());
    Batchslot c(a);
    Maptype::iterator a_iter;
    while (true) {
        if (b == Batchslot(0))
            return {};
        HashView hv { t[c] };
        auto iter = headers.find(hv);
        if (iter == headers.end())
            b = c;
        else {
            a = c;
            a_iter = iter;
        }
        if (b - a == 1) {
            return SharedBatchView(a_iter);
        }
        c = a + (b - a) / 2;
    }
}

std::optional<SharedBatch> BatchRegistry::find_last(const HashGrid g, const std::optional<SignedSnapshot>& ss)
{
    std::unique_lock l(m);
    auto res { find_last_template(g) };
    if (ss.has_value() && !verify(res, *ss)) {
        return {};
    }
    return static_cast<SharedBatch>(res);
}

void BatchRegistry::dec_ref(SharedBatch::iter_type iter)
{
    std::unique_lock l(m);
    while (true) {
        Nodedata& nd = iter->second;
        nd.refcount -= 1;
        if (nd.refcount > 0)
            break;
        auto tmp = nd.prev.data;
        nd.prev.data.raw = 0;
        headers.erase(iter);
        if (tmp.raw == 0)
            break;
        iter = tmp.iter;
    }
}

bool BatchRegistry::verify(SharedBatchView v, const SignedSnapshot& ss)
{
    if (!v.valid())
        return true;
    auto iter { v.data.iter };
    auto& node { iter->second };
    auto hash { node.hash_at(ss.height()) };
    if (!hash)
        return true;
    return *hash == ss.hash;
}

Nodedata::~Nodedata()
{
    assert(refcount == 0);
}

std::optional<Hash> Nodedata::hash_at(NonzeroHeight h)
{
    auto uh { upper_height() };
    if (uh == h) {
        return batch.get_header(h - lower_height()).value().hash();
    }
    auto h1 { h + 1 };
    auto pnode = this;
    if (uh < h1)
        return {};
    while (true) {
        auto lh { pnode->lower_height() };
        if (lh <= h1) {
            return Hash { pnode->batch.get_header(h1 - lh)->prevhash() };
        }
        assert(pnode->prev.valid());
        pnode = &pnode->prev.data.iter->second;
        uh = pnode->upper_height() + 1;
        assert(uh == lh);
    }
}
