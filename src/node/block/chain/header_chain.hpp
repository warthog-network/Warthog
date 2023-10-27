#pragma once
#include "block/chain/fork_range.hpp"
#include "block/header/view_inline.hpp"
#include "communication/messages.hpp"
#include "api/types/forward_declarations.hpp"

struct HeaderchainAppend {
    std::vector<SharedBatchView> completeBatches;
    SharedBatch finalPin;
    Batch incompleteBatch;
};

struct HeaderchainRollback {
    Height shrinkLength;
    Descriptor descriptor;
};

struct HeaderchainFork {
    std::vector<SharedBatchView> completeBatches;
    SharedBatch finalPin;
    Batch incompleteBatch;
    Height shrinkLength;
    Descriptor descriptor;
};

class HeaderchainSkeleton {
public:
    HeaderchainSkeleton(SharedBatch finalPin, Batch incompleteBatch)
        : finalPin(std::move(finalPin))
        , incompleteBatch(std::move(incompleteBatch)) {};

    std::optional<HeaderView> inefficient_get_header(NonzeroHeight h) const;
    Height length() const { return finalPin.upper_height() + incompleteBatch.size(); };
    const Batch& incomplete_batch() { return incompleteBatch; }

protected:
    HeaderchainSkeleton() {};
    SharedBatch finalPin;
    Batch incompleteBatch;
};

class Headerchain;
[[nodiscard]] ForkHeight fork_height(const Headerchain& h1, const Headerchain& h2, NonzeroHeight startHeight = { 1 });
class Headerchain : public HeaderchainSkeleton {
    struct HeaderViewNoHash : public HeaderView {
        HeaderViewNoHash(const HeaderView& hv)
            : HeaderView(hv) {};
        Hash hash() = delete;
    };

public:
    struct pin_t {
        Headerchain& operator*() const
        {
            return **data;
        }
        Headerchain* operator->() const
        {
            return &**data;
        }
        operator bool() const
        {
            return data.use_count() != 0;
        }

        std::shared_ptr<std::shared_ptr<Headerchain>> data;
    };

    struct GridView {
        const std::vector<SharedBatchView>& vec;
        HeaderView operator[](Batchslot s) const { return vec[s.index()].getBatch().last(); }
        HeaderView operator[](size_t i) const { return vec[i].getBatch().last(); }
        size_t size() const { return vec.size(); }
        Batchslot slot_end() const { return Batchslot(size()); }
        GridView(const std::vector<SharedBatchView>& vec)
            : vec(vec) {};
    };
    // chain updates
    [[nodiscard]] HeaderchainAppend get_append(Height prevLength) const;
    [[nodiscard]] std::pair<Height, AppendMsg> apply_append(HeaderchainAppend&& append);
    [[nodiscard]] HeaderchainFork get_fork(NonzeroHeight forkHeight, Descriptor descriptor) const;
    [[nodiscard]] ForkMsg apply_fork(HeaderchainFork&& fork);

    void shrink(Height shrinkLength);
    uint64_t hashrate(uint32_t nblocks) const;
    API::HashrateChart hashrate_chart(NonzeroHeight min, NonzeroHeight max, uint32_t nblocks) const;

    size_t nonempty_batch_size() const { return completeBatches.size() + (incompleteBatch.size() > 0 ? 1 : 0); }
    Batch get_headers(NonzeroHeight begin, NonzeroHeight end) const;
    GridView grid_view() const { return completeBatches; }
    std::optional<HeaderView> get_header(Height) const;
    [[nodiscard]] Height length() const
    {
        return finalPin.upper_height() + incompleteBatch.size();
    }
    Headerchain() {};
    explicit Headerchain(HeaderchainSkeleton);
    Headerchain(Headerchain&& hc) = default;
    Headerchain(const Headerchain& hc) = default;
    Headerchain(const Headerchain& hc, Height subheight);
    Headerchain& operator=(const Headerchain&) = default;
    Headerchain& operator=(Headerchain&&) = default;
    const HeaderViewNoHash operator[](NonzeroHeight) const;
    Grid grid(Batchslot begin = Batchslot(0)) const;
    const Batch* operator[](Batchslot bs) const
    {
        size_t index = bs.index();
        if (index > completeBatches.size())
            return nullptr;
        if (index == completeBatches.size()) {
            return &incompleteBatch;
        }
        return &completeBatches[index].getBatch();
    };
    Worksum total_work() const { return worksum; }
    const std::vector<SharedBatchView>& complete_batches() const { return completeBatches; }
    [[nodiscard]] Worksum total_work_at(Height) const;
    [[nodiscard]] std::optional<Hash> get_hash(Height h) const
    {
        if (h > length())
            return {};
        if (length() == 0)
            return Hash::genesis();
        if (h == length())
            return static_cast<HeaderView>(operator[](h.nonzero_assert())).hash();
        return operator[]((h + 1).nonzero_assert()).prevhash();
    };
    [[nodiscard]] Hash hash_at(Height height) const
    {
        auto h = get_hash(height);
        assert(h);
        return *h;
    };

    void clear();
    friend ForkHeight fork_height(const Headerchain& h1, const Headerchain& h2, NonzeroHeight startHeight);

protected: // methods
    const HeaderView header_view(uint32_t height) const;
    void initialize_worksum();
    [[nodiscard]] Worksum sum_work(const Height begin, const Height end) const;

protected: // variables
    std::vector<SharedBatchView> completeBatches;
    Worksum worksum;
};
