#include "header_chain.hpp"
#include "api/types/all.hpp"
#include "block/chain/binary_forksearch.hpp"
#include "block/header/view_inline.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/is_testnet.hpp"
#include <algorithm>
#include <stdexcept>
using namespace std;

std::optional<HeaderView> HeaderchainSkeleton::inefficient_get_header(NonzeroHeight h) const
{
    const SharedBatch* p = &finalPin;
    const Batch* b = &incompleteBatch;
    Height bStart { p->upper_height() + 1 };
    while (h <= p->upper_height()) {
        // assert(p->slot().has_value());
        bStart = p->lower_height();
        b = &p->getBatch();
        p = &p->prev();
    }
    assert(h >= bStart);
    return b->get_header(h - bStart);
}

HeaderchainAppend Headerchain::get_append(Height prevLength) const
{
    return {
        .completeBatches {
            completeBatches.begin() + prevLength.value() / HEADERBATCHSIZE,
            completeBatches.end() },
        .finalPin { finalPin },
        .incompleteBatch { incompleteBatch }
    };
}

std::pair<Height, AppendMsg> Headerchain::apply_append(HeaderchainAppend&& update)
{
    Worksum prevWorksum = worksum;
    Height h(length());
    assert(update.completeBatches.size() > 0 || update.incompleteBatch.size() > 0);
    const Batchslot batchOffset { uint32_t(completeBatches.size()) };
    completeBatches.insert(completeBatches.end(),
        update.completeBatches.begin(),
        update.completeBatches.end());
    incompleteBatch = std::move(update.incompleteBatch);
    finalPin = std::move(update.finalPin);
    initialize_worksum();
    assert(worksum > prevWorksum);
    return { h, { length().nonzero_assert(), worksum, grid(batchOffset) } };
}

HeaderchainFork Headerchain::get_fork(NonzeroHeight forkHeight, Descriptor descriptor) const
{
    assert(forkHeight < length());
    auto shrinkLength { forkHeight - 1 };
    return HeaderchainFork {
        .completeBatches {
            completeBatches.begin() + shrinkLength.complete_batches(),
            completeBatches.end() },
        .finalPin { finalPin },
        .incompleteBatch = incompleteBatch,
        .shrinkLength = shrinkLength,
        .descriptor = descriptor
    };
}

ForkMsg Headerchain::apply_fork(HeaderchainFork&& update)
{
    Worksum prevWorksum = worksum;
    assert(update.shrinkLength < length());
    assert(update.completeBatches.size() > 0 || update.incompleteBatch.size() > 0);

    size_t nComplete = update.shrinkLength.complete_batches();
    completeBatches.erase(completeBatches.begin() + nComplete, completeBatches.end());

    assert(completeBatches.size() == nComplete);
    const Batchslot batchOffset { uint32_t(completeBatches.size()) };
    completeBatches.insert(completeBatches.end(),
        update.completeBatches.begin(),
        update.completeBatches.end());
    incompleteBatch.swap(update.incompleteBatch);
    finalPin = std::move(update.finalPin);
    initialize_worksum();
    assert(update.shrinkLength == length());
    assert(worksum > prevWorksum);
    return ForkMsg(
        update.descriptor,
        length().nonzero_assert(),
        worksum,
        (update.shrinkLength + 1).nonzero_assert(),
        grid(batchOffset));
}

void Headerchain::shrink(Height shrinkLength)
{
    Worksum prevWorksum = worksum;
    assert(shrinkLength < length());

    size_t nIncomplete = shrinkLength.incomplete_batch_size();
    size_t nComplete = shrinkLength.complete_batches();
    if (nComplete == completeBatches.size()) {
        incompleteBatch.shrink(nIncomplete);
    } else {
        assert(nComplete < completeBatches.size());
        incompleteBatch = completeBatches[nComplete].getBatch();
        incompleteBatch.shrink(nIncomplete);
        completeBatches.erase(completeBatches.begin() + nComplete, completeBatches.end());
        if (nComplete > 0) {
            finalPin = completeBatches.back();
        } else {
            finalPin = SharedBatch {};
        }
    }
    initialize_worksum();
    assert(worksum < prevWorksum);
}

uint64_t Headerchain::hashrate(uint32_t nblocks) const
{
    if (length() < Height(2))
        return 0;
    NonzeroHeight lower { length().value() > nblocks ? (length() - nblocks).nonzero_assert() : NonzeroHeight { 1u } };
    NonzeroHeight upper { length().nonzero_assert() };
    auto ltime { operator[](lower).timestamp() };
    auto utime { operator[](upper).timestamp() };
    if (ltime >= utime)
        return std::numeric_limits<uint64_t>::max();
    auto seconds { utime - ltime };
    auto nBlocks { upper - lower };
    assert(nBlocks > 0);
    return sum_work(lower + 1, upper + 1).getdouble() / seconds;
}

API::HashrateChart Headerchain::hashrate_chart(NonzeroHeight reqmin, NonzeroHeight reqmax, const uint32_t nblocks) const
{
    const auto max { std::min(Height(reqmax), length()) };
    const auto min { std::max(reqmin, NonzeroHeight(2u)) };
    if (max < min)
        return { .range { .begin { min }, .end { max } }, .chart {} };

    std::vector<double> chart;

    NonzeroHeight lower { min.value() > nblocks ? (min - nblocks).nonzero_assert() : NonzeroHeight { 1u } };
    NonzeroHeight upper { min };
    auto worksum { sum_work(lower + 1, upper + 1) };
    auto ltime { operator[](lower).timestamp() };
    auto utime { operator[](upper).timestamp() };
    auto compute_hashrate = [&ltime, &utime, &lower, &upper, &worksum]() {
        if (ltime >= utime)
            return double(std::numeric_limits<uint64_t>::max());
        auto seconds { utime - ltime };
        auto nBlocks { upper - lower };
        assert(nBlocks > 0);
        return worksum.getdouble() / seconds;
    };
    chart.push_back(compute_hashrate());
    for (auto h { min + 1 }; h <= max; ++h) {
        NonzeroHeight l { h.value() > nblocks ? (h - nblocks).nonzero_assert() : NonzeroHeight { 1u } };
        NonzeroHeight u { h };
        ltime = operator[](l).timestamp();
        utime = operator[](u).timestamp();
        if (l != lower) {
            assert(l == lower + 1);
            worksum -= operator[](l).target(l, is_testnet());
            lower = l;
        }
        if (u != upper) {
            assert(u == upper + 1);
            worksum += operator[](upper).target(upper, is_testnet());
            upper = u;
        }
        chart.push_back(compute_hashrate());
    }
    assert(chart.size() == max - min + 1);
    assert(chart.size() != 0);
    return { .range { .begin { min }, .end { max } }, .chart { std::move(chart) } };
}

Batch Headerchain::get_headers(BlockRange range) const
{
    assert(range.length() <= HEADERBATCHSIZE);
    auto end { range.end() };
    if (end > length()) {
        end = (length() + 1).nonzero_assert();
    }
    Height h { range.lower() };

    if (end <= h)
        return Batch {};

    std::vector<uint8_t> tmp;
    while (h < end) {
        Batchslot bs(h);
        const Batch* p = operator[](bs);
        if (!p)
            break;
        uint32_t offset = h - bs.lower();
        auto cpy_begin = p->data() + Header::byte_size() * offset;
        uint32_t n1 = end - h;
        uint32_t n2 = bs.upper() + 1 - h;
        uint32_t n;
        if (n1 <= n2) {
            n = n1;
        } else {
            n = n2;
        }
        h = h + n;
        auto cpy_end = cpy_begin + n * Header::byte_size();
        assert(size_t(cpy_end - p->data()) <= p->size() * Header::byte_size());
        std::copy(cpy_begin, cpy_end, std::back_inserter(tmp));
    }
    return Batch(std::move(tmp));
}

std::optional<HeaderView> Headerchain::get_header(Height h) const
{
    if (h == 0 || h > length())
        return {};
    auto s = Batchslot(h);
    size_t i = s.index();
    size_t rem = h - s.lower();
    if (i < completeBatches.size()) {
        return completeBatches[i].getHeader(rem);
    } else {
        return incompleteBatch.get_header(rem);
    }
}

ForkHeight fork_height(const Headerchain& h1, const Headerchain& h2, NonzeroHeight startHeight)
{
    Batchslot bs(startHeight);
    auto [f, _] = binary_forksearch(h1.completeBatches, h2.completeBatches, bs.index());
    const Batch& b1 = (f < h1.completeBatches.size() ? h1.completeBatches[f].getBatch() : h1.incompleteBatch);
    const Batch& b2 = (f < h2.completeBatches.size() ? h2.completeBatches[f].getBatch() : h2.incompleteBatch);
    auto [forkIndex, forked] = binary_forksearch(b1, b2);
    return { NonzeroHeight(uint32_t(f * HEADERBATCHSIZE + forkIndex + 1)), forked };
}

std::optional<NonzeroHeight> Headerchain::max_match_height(const HeaderRange& hrange) const
{
    std::optional<NonzeroHeight> h;
    for (auto header1 : hrange) {
        auto header2 { get_header(header1.height) };
        if (header2 != header1)
            return h;
        h = header1.height;
    }
    return h;
}

Headerchain::Headerchain(HeaderchainSkeleton skeleton)
    : HeaderchainSkeleton(std::move(skeleton))
{
    const SharedBatch* p = &finalPin;
    while (p->valid()) {
        completeBatches.push_back(SharedBatchView(*p));
        p = &p->prev();
    }
    std::reverse(completeBatches.begin(), completeBatches.end());
    initialize_worksum();
}
Headerchain::Headerchain(const Headerchain& from, Height subheight)
{
    if (subheight > from.length())
        throw std::out_of_range("Cannot extract subchain of length " + to_string(subheight) + " from chain of length " + to_string(from.length()));
    Batchslot bs(subheight);
    const size_t I = bs.index() + 1;
    for (size_t i = 0; i < I; ++i) {
        completeBatches.push_back(from.completeBatches[i]);
    }
    const Batch& b = (from.completeBatches.size() == I ? from.incompleteBatch : from.completeBatches[I].getBatch());
    incompleteBatch = b;
    incompleteBatch.shrink(subheight - bs.lower());
    initialize_worksum();
}

const Headerchain::HeaderViewNoHash Headerchain::operator[](NonzeroHeight h) const
{
    if (h > length())
        throw std::out_of_range("Headerchain has length " + to_string(length()) + ". Cannot access index " + to_string(h));
    Batchslot bs(h);
    size_t i = bs.index();
    size_t rem = h - bs.lower();
    assert(((h - 1).value() % HEADERBATCHSIZE) == rem);
    if (i < completeBatches.size()) {
        return static_cast<Headerchain::HeaderViewNoHash>(completeBatches[i].getBatch()[rem]);
    } else {
        return incompleteBatch[rem];
    }
}

Grid Headerchain::grid(Batchslot begin) const
{
    return { *this, begin };
}

void Headerchain::initialize_worksum()
{
    assert(Height(completeBatches.size() * HEADERBATCHSIZE) == finalPin.upper_height());
    worksum = incompleteBatch.worksum(finalPin.upper_height());
    if (completeBatches.size() > 0) {
        worksum += completeBatches.back().total_work();
    }
    auto ws2 = sum_work(NonzeroHeight(1u), (length() + 1).nonzero_assert());
    assert(worksum == ws2);
}

Worksum Headerchain::sum_work(const NonzeroHeight beginHeight,
    const NonzeroHeight endHeight) const
{
    if (beginHeight >= endHeight)
        return {};
    Worksum sum;
    assert(endHeight <= length() + 1);
    Height upperHeight = (endHeight - 1).nonzero_assert();
    bool complete = false;
    while (!complete) {
        auto header = get_header(upperHeight);
        assert(header);
        Worksum w(header->target(upperHeight.nonzero_assert(), is_testnet()));
        Height lower = (upperHeight - 1).retarget_floor();
        if (lower == 1) {
            lower = Height(0);
            complete = true;
        } else if (lower < beginHeight) {
            lower = beginHeight - 1;
            complete = true;
        }
        w *= (upperHeight - lower);
        sum += w;
        upperHeight = lower;
    }
    return sum;
}

[[nodiscard]] Worksum Headerchain::total_work_at(Height h) const
{
    if (h == 0)
        return {};
    assert(h <= length());
    Batchslot s(h);
    SharedBatchView prev(s == Batchslot(0) ? SharedBatchView() : completeBatches[s.index() - 1]);
    auto& incomplete { s.index() == completeBatches.size() ? incompleteBatch : completeBatches[s.index()].getBatch() };
    assert(s.offset() == prev.upper_height());
    Worksum w(prev.total_work() + incomplete.worksum(s.offset(), h - s.offset()));
    assert(w == sum_work(NonzeroHeight(1u), (h + 1).nonzero_assert()));
    return w;
}

void Headerchain::clear()
{
    completeBatches.clear();
    incompleteBatch.clear();
    worksum.setzero();
}
