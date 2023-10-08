#include "consensus_headers.hpp"
#include "general/now.hpp"

HeaderVerifier::HeaderVerifier(const SharedBatch& b)
{
    length = b.upper_height();
    finalHash = b.getBatch().last().hash();
    latestRetargetHeight = length.retarget_floor();

    // find latestRetarget header
    const SharedBatch* tmp = &b;
    while (tmp->lower_height() > latestRetargetHeight) {
        tmp = &tmp->prev();
    }
    HeaderView uhv = tmp->getBatch()[latestRetargetHeight - tmp->lower_height()];
    latestRetargetTime = uhv.timestamp();

    timeValidator.clear();
    if (latestRetargetHeight == 1) {
        nextTarget = Target::genesis();
    } else {

        nextTarget = uhv.target();

        // find prevRetarget header
        Height prevRetargetHeight = (latestRetargetHeight - 1).retarget_floor();
        while (tmp->lower_height() > prevRetargetHeight) {
            tmp = &tmp->prev();
        }
        HeaderView lhv = tmp->getBatch()[prevRetargetHeight - tmp->lower_height()];
        uint32_t prevRetargetTime = lhv.timestamp();

        assert(prevRetargetHeight < length);
        nextTarget.scale(latestRetargetTime - prevRetargetTime,
            BLOCKTIME * (latestRetargetHeight - prevRetargetHeight));
        if (b.upper_height() > latestRetargetHeight) {
            assert(b.getBatch().last().target() == nextTarget);
        }
    }
    static_assert(MEDIAN_N < HEADERBATCHSIZE);
    assert(MEDIAN_N < b.size());
    for (size_t i = 0; i < MEDIAN_N; ++i) {
        timeValidator.append(b.size() - MEDIAN_N + i);
    }
}

tl::expected<HeaderVerifier, ChainError> HeaderVerifier::copy_apply(const std::optional<SignedSnapshot>& sp, const Batch& b, Height heightOffset) const
{
    HeaderVerifier res { *this };
    assert(heightOffset == length);
    for (size_t i = 0; i < b.size(); ++i) {
        auto e { res.prepare_append(sp, b[i]) };
        auto height { (heightOffset + 1 + i).nonzero_assert() };
        if (!e.has_value()) {
            return tl::make_unexpected(ChainError(e.error(), height));
        }
        res.append(height, e.value());
    }
    return res;
}

void HeaderVerifier::clear()
{
    length = Height(0);
    latestRetargetHeight = Height(0);
    latestRetargetTime = 0;
    nextTarget = Target::genesis();
    timeValidator.clear();
    finalHash = Hash::genesis();
}

void HeaderVerifier::append(NonzeroHeight newlength, const PreparedAppend& p)
{
    assert(newlength == length + 1);
    length = newlength;
    // adjust height and hash
    finalHash = p.hash;

    // adjust timestamp validator
    uint32_t timestamp = p.hv.timestamp();
    assert(timestamp != 0);
    timeValidator.append(timestamp);

    // adjust next Target
    const Height upperHeight = newlength.retarget_floor();
    if (upperHeight == newlength) {
        if (upperHeight == 1) {
            latestRetargetHeight = Height(1);
            latestRetargetTime = timestamp;
        } else {
            assert(latestRetargetHeight != 0);
            assert(latestRetargetTime != 0);
            assert(latestRetargetTime < timestamp);
            Height lowerHeight((upperHeight - 1).retarget_floor());
            assert(upperHeight - lowerHeight > 0);
            nextTarget.scale(timestamp - latestRetargetTime,
                BLOCKTIME * (upperHeight - lowerHeight));
            latestRetargetHeight = upperHeight;
            latestRetargetTime = timestamp;
        }
    }
}

auto HeaderVerifier::prepare_append(const std::optional<SignedSnapshot>& sp, HeaderView hv) const -> tl::expected<PreparedAppend, int32_t>
{

    // Check header link
    if (hv.prevhash() != finalHash) {
        return tl::make_unexpected(EHEADERLINK);
    }

    // Check difficulty
    if (hv.target() != nextTarget)
        return tl::make_unexpected(EDIFFICULTY);

    // Check POW
    if (!hv.validPOW()) {
        return tl::make_unexpected(EPOW);
    }

    // Check signed pin
    auto hash { hv.hash() };
    if (sp && length + 1 == sp->priority.height && sp->hash != hash)
        return tl::make_unexpected(ELEADERMISMATCH);

    const uint32_t t = hv.timestamp();

    // Check increasing median
    // Check no time drops (should be automatically valid if no future times)
    if (!timeValidator.valid(t)
        || latestRetargetTime >= t)
        return tl::make_unexpected(ETIMESTAMP);

    // Check no future block times
    // LATER: use network time
    if (t > now_timestamp() + TOLERANCEMINUTES * 60)
        return tl::make_unexpected(ECLOCKTOLERANCE);
    return PreparedAppend { hv, hash };
}

void HeaderVerifier::initialize(const ExtendableHeaderchain& hc,
    Height length)
{
    finalHash = hc.hash_at(length);

    this->length = length;
    //////////////////////////////
    // time validator
    //////////////////////////////
    timeValidator.clear();
    // now fill timestamp vaildator
    if (hc.incompleteBatch.size() >= timeValidator.N) {
        for (size_t i = hc.incompleteBatch.size() - timeValidator.N;
             i < hc.incompleteBatch.size(); ++i) {
            timeValidator.append(hc.incompleteBatch[i].timestamp());
        }
    } else {
        if (hc.completeBatches.size() > 0) {
            const SharedBatchView& sb = hc.completeBatches.back();
            size_t rem = timeValidator.N - hc.incompleteBatch.size();
            for (size_t i = sb.size() - rem; i < sb.size(); ++i)
                timeValidator.append(sb.getBatch()[i].timestamp());
        }
        for (size_t i = 0; i < hc.incompleteBatch.size(); ++i)
            timeValidator.append(hc.incompleteBatch[i].timestamp());
    }

    //////////////////////////////
    // initialize target
    //////////////////////////////
    Height upperHeight = length.retarget_floor();
    if (length == 0) {
        nextTarget = Target::genesis();
        latestRetargetHeight = Height(0);
        latestRetargetTime = 0;
    } else {
        if (upperHeight == 1) {
            nextTarget = Target::genesis();
            latestRetargetHeight = upperHeight;
            latestRetargetTime = hc[NonzeroHeight(1)].timestamp();
        } else {
            Height lowerHeight = (upperHeight - 1).retarget_floor();
            auto lower { hc.get_header(lowerHeight) };
            auto upper { hc.get_header(upperHeight) };
            assert(lower);
            assert(upper);
            assert(upperHeight - lowerHeight > 0);
            nextTarget = upper->target();
            latestRetargetTime = upper->timestamp();
            latestRetargetHeight = upperHeight;
            nextTarget.scale(latestRetargetTime - lower->timestamp(),
                BLOCKTIME * (upperHeight - lowerHeight));
        }
    }
}

void ExtendableHeaderchain::initialize()
{
    initialize_worksum();
    checker.initialize(*this, length());
}

void ExtendableHeaderchain::shrink(Height newlength)
{
    assert(newlength <= length());
    size_t numComplete = newlength.complete_batches();
    if (numComplete == completeBatches.size()) {
        // only need to shrink incompleteBatch
        incompleteBatch.shrink(newlength.incomplete_batch_size());
    } else {
        incompleteBatch = completeBatches[numComplete].getBatch();
        incompleteBatch.shrink(newlength.incomplete_batch_size());
        completeBatches.erase(completeBatches.begin() + numComplete,
            completeBatches.end());
        if (completeBatches.size() > 0)
            finalPin = completeBatches.back();
        else
            finalPin = SharedBatch();
    }
    initialize();
}

ExtendableHeaderchain::ExtendableHeaderchain()
{
    initialize();
}

ExtendableHeaderchain::ExtendableHeaderchain(
    std::vector<Batch>&& init,
    BatchRegistry& br)
{
    // p.first
    Height incompleteHeightOffset { 0 };
    Worksum totalWork;
    for (size_t i = 0; i < init.size(); ++i) {
        incompleteBatch = std::move(init[i]);
        if (incompleteBatch.complete()) {
            finalPin = br.share(std::move(incompleteBatch), finalPin);
            completeBatches.push_back(finalPin);
            incompleteBatch.clear();
            incompleteHeightOffset = finalPin.upper_height();
            totalWork = finalPin.total_work();
        }
    }
    totalWork += incompleteBatch.worksum(incompleteHeightOffset);
    initialize();
    assert(totalWork == total_work());
}

ExtendableHeaderchain::ExtendableHeaderchain(const Headerchain& eh,
    Height height)
    : Headerchain(eh, height)
{
    initialize();
}
ExtendableHeaderchain::ExtendableHeaderchain(Headerchain&& hc)
    : Headerchain(std::move(hc))
{
    initialize();
}

ExtendableHeaderchain& ExtendableHeaderchain::operator=(Headerchain&& hc)
{
    Headerchain::operator=(std::move(hc));
    initialize();
    return *this;
}

void ExtendableHeaderchain::append(const HeaderVerifier::PreparedAppend& p,
    BatchRegistry& br)
{
    worksum += p.hv.target();
    incompleteBatch.append(p.hv);
    if (incompleteBatch.complete()) {
        finalPin = br.share(std::move(incompleteBatch), finalPin, worksum);
        completeBatches.push_back(finalPin);
        incompleteBatch.clear();
    }
    checker.append(length().nonzero_assert(), p);
}

auto ExtendableHeaderchain::prepare_append(const std::optional<SignedSnapshot>& sp, HeaderView hv) const -> tl::expected<HeaderVerifier::PreparedAppend, int32_t>
{
    return checker.prepare_append(sp, hv);
}

MiningData ExtendableHeaderchain::mining_data() const
{
    return {
        (length() + 1).reward(),
        final_hash(),
        next_target(),
        checker.get_valid_timestamp()
    };
}
