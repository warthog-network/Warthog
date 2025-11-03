#pragma once
#include "batch.hpp"
#include "block/chain/worksum.hpp"
#include <mutex>

class BatchRegistry;
struct Nodedata;
class HeaderVerifier;
struct SignedSnapshot;
class SharedBatchView {
    friend class BatchRegistry;
    using Maptype = std::map<std::array<uint8_t, 80>, Nodedata, HeaderView::HeaderComparator>;
    using iter_type = Maptype::iterator;
    friend class SharedBatch;

public:
    SharedBatchView()
        : data { .raw = 0 } { };
    bool valid() const { return data.raw != 0; }
    size_t size() const;
    Height upper_height() const;
    Height lower_height() const;
    wrt::optional<Batchslot> slot() const;
    wrt::optional<HeaderView> getHeader(size_t id) const;
    const Batch& getBatch() const;
    Worksum total_work() const;
    bool operator==(const SharedBatchView& rhs) const;

private:
    SharedBatchView(iter_type iter)
        : data { .iter = iter } { };
    union { // @Rafiki What is this? Emulate nullable iterator? This is undefined behavior! :( But I think it should work this way because iter will be actually a pointer and non-zero if it points to something useful.
        iter_type iter;
#ifdef DISABLE_LIBUV
        uint32_t raw; // emscripten build has 32 bytes iterator size
#else
        uint64_t raw;
#endif
        static_assert(sizeof(iter) == sizeof(raw));
    } data;
};

class SharedBatch {
    using Maptype = std::map<std::array<uint8_t, 80>, Nodedata, HeaderView::HeaderComparator>;
    using iter_type = Maptype::iterator;
    friend struct Nodedata;

public:
    ~SharedBatch();
    SharedBatch()
        : data({ .raw = 0 })
    {
    }
    SharedBatch(const SharedBatch& other);
    SharedBatch(const SharedBatchView&) noexcept;
    SharedBatch(SharedBatch&& other) noexcept;
    SharedBatch& operator=(const SharedBatch& other);
    SharedBatch& operator=(SharedBatch&& other);
    SharedBatch& operator=(const SharedBatchView& other);
    operator SharedBatchView() const { return view(); }
    bool operator==(const SharedBatch& p2) const;
    size_t size() const { return view().size(); }
    Height upper_height() const { return view().upper_height(); }
    wrt::optional<Batchslot> slot() const { return view().slot(); }
    Batchslot next_slot() const { return slot().value_or(Batchslot(0)) + 1; }
    Height lower_height() const { return view().lower_height(); }
    wrt::optional<HeaderView> getHeader(size_t id) const { return view().getHeader(id); }
    [[nodiscard]] wrt::optional<HeaderView> search_header_recursive(NonzeroHeight h) const;
    [[nodiscard]] HeaderView operator[](Height h) const { return getHeader(h - lower_height()).value(); }
    const Batch& getBatch() const { return view().getBatch(); }
    const Worksum total_work() const { return view().total_work(); }
    HeaderVerifier verifier() const;
    const SharedBatch& prev() const;
    bool valid() const { return view().valid(); }

private:
    SharedBatchView view() const { return static_cast<SharedBatchView::iter_type>(data.iter); }
    friend class BatchRegistry;
    SharedBatch(iter_type iter) noexcept;

private: // private data;
    union U {
        iter_type iter;
        uint64_t raw;
    } data;
    static_assert(sizeof(U) == sizeof(uint64_t));
};

struct Nodedata {
    Nodedata(BatchRegistry& registry, Batch&& headerbatch,
        const Worksum& totalWork, SharedBatch&& parent)
        : registry(registry)
        , batch(std::move(headerbatch))
        , totalWork(totalWork)
        , prev(std::move(parent))
        , slot { prev.valid() ? prev.slot().value() + 1 : Batchslot(0) }
    {
    }
    ~Nodedata();
    wrt::optional<Hash> hash_at(NonzeroHeight);
    Height upper_height() const
    {
        return slot.upper();
    }
    Height lower_height() const
    {
        return slot.lower();
    }
    int64_t refcount = 0;
    BatchRegistry& registry;
    Batch batch;
    Worksum totalWork;
    SharedBatch prev;
    Batchslot slot;
};

inline bool SharedBatchView::operator==(const SharedBatchView& rhs) const
{
    return data.iter == rhs.data.iter;
}
inline bool SharedBatch::operator==(const SharedBatch& p2) const
{
    return data.iter == p2.data.iter;
}
inline size_t SharedBatchView::size() const { return (valid() ? data.iter->second.batch.size() : 0); }
inline wrt::optional<HeaderView> SharedBatchView::getHeader(size_t id) const
{
    if (valid()) {
        return data.iter->second.batch.get_header(id);
    }
    return {};
}

inline wrt::optional<Batchslot> SharedBatchView::slot() const
{
    if (valid()) {
        return data.iter->second.slot;
    }
    return {};
}

inline const Batch& SharedBatchView::getBatch() const
{
    assert(valid());
    return data.iter->second.batch;
}
inline Worksum SharedBatchView::total_work() const
{
    return (valid() ? data.iter->second.totalWork : Worksum {});
}
inline Height SharedBatchView::upper_height() const
{
    if (valid())
        return data.iter->second.upper_height();
    else
        return Height { 0 };
}
inline Height SharedBatchView::lower_height() const
{
    if (valid())
        return data.iter->second.slot.lower();
    else
        return Height { 0 };
}
inline const SharedBatch& SharedBatch::prev() const
{
    assert(valid());
    return data.iter->second.prev;
}

struct HeaderSearchRecursive {
    HeaderSearchRecursive(const SharedBatch& sb, const Batch& b)
        : psb(&sb)
        , pb(&b)
    {
    }

    wrt::optional<HeaderView> find_prev(NonzeroHeight h)
    {
        Height bStart { psb->upper_height() + 1 };
        while (h <= psb->upper_height()) {
            bStart = psb->lower_height();
            pb = &psb->getBatch();
            psb = &psb->prev();
        }
        assert(h >= bStart);
        return pb->get_header(h - bStart);
    }

private:
    const SharedBatch* psb;
    const Batch* pb;
};

class BatchRegistry {
    friend class SharedBatch;
    using Maptype = std::map<std::array<uint8_t, 80>, Nodedata, HeaderView::HeaderComparator>;

public:
    ~BatchRegistry()
    {
        assert(headers.size() == 0);
    }
    [[nodiscard]] SharedBatch share(Batch&& headerbatch, const SharedBatch& prev);
    [[nodiscard]] SharedBatch share(Batch&& headerbatch, const SharedBatch& prev, Worksum totalWork);
    wrt::optional<SharedBatch> find_last(const Grid g, const wrt::optional<SignedSnapshot>&);
    // wrt::optional<SharedBatch> findLast(const std::vector<Batch>& batches, const wrt::optional<SignedSnapshot>&);

private: // private methods
    template <typename T>
    SharedBatchView find_last_template(const T& batches);
    void dec_ref(SharedBatch::iter_type iter);
    bool verify(SharedBatchView, const SignedSnapshot&);

private: // private data
    std::recursive_mutex m;
    Maptype headers;
};
