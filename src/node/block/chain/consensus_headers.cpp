#include "consensus_headers.hpp"
#include "general/is_testnet.hpp"
#include "general/now.hpp"
#include "block/header/difficulty_scale.hpp"
#include "spdlog/spdlog.h"

HeaderVerifier::HeaderVerifier(const SharedBatch& b)
    : nextTarget(TargetV1())
{
    length = b.upper_height();
    HeaderView finalHeader { b.getBatch().last() };
    finalHash = finalHeader.hash();
    latestRetargetHeight = length.retarget_floor();

    // find latestRetarget header
    const SharedBatch* tmp = &b;
    while (tmp->lower_height() > latestRetargetHeight) {
        tmp = &tmp->prev();
    }
    HeaderView uhv = tmp->getBatch()[latestRetargetHeight - tmp->lower_height()];
    latestRetargetTime = uhv.timestamp();

    timeValidator.clear();
    bool override = false;
    if (JANUSENABLED && !is_testnet()) {
        if (length == JANUSRETARGETSTART) {
            override = true;
            nextTarget = TargetV2::initial();
        }
        if (length == JANUSV2RETARGETSTART) {
            override = true;
            nextTarget = TargetV2::initialv2();
        }
    }
    if (!override) {
        if (latestRetargetHeight == 1) {
            if (is_testnet()) 
                nextTarget = TargetV2::genesis_testnet();
            else
                nextTarget = TargetV1::genesis();
        } else {
            nextTarget = finalHeader.target(length.nonzero_assert(), is_testnet());
            if (length.retarget_floor() == length) { // need retarget
                auto prevRetargetHeight = (length - 1).retarget_floor();
                while (tmp->lower_height() > prevRetargetHeight) {
                    tmp = &tmp->prev();
                }
                HeaderView lhv = tmp->getBatch()[prevRetargetHeight - tmp->lower_height()];
                nextTarget.scale(finalHeader.timestamp() - lhv.timestamp(),
                    BLOCKTIME * (length - prevRetargetHeight), length + 1);
            }
        }
    }
    static_assert(MEDIAN_N < HEADERBATCHSIZE);
    assert(MEDIAN_N < b.size());
    for (size_t i = 0; i < MEDIAN_N; ++i) {
        timeValidator.append(b.getBatch()[b.size() - MEDIAN_N + i].timestamp());
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

HeaderVerifier::HeaderVerifier()
    : nextTarget(TargetV1::genesis())
{
    length = Height(0);
    latestRetargetHeight = Height(0);
    latestRetargetTime = 0;
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
    const uint32_t timestamp = p.hv.timestamp();
    assert(timestamp != 0);
    timeValidator.append(timestamp);

    // adjust next Target
    const Height upperHeight = newlength.retarget_floor();
    static_assert(::retarget_floor(JANUSRETARGETSTART) == JANUSRETARGETSTART);
    using namespace std;
    if (upperHeight == newlength) { // need retarget
        bool override = false;
        if (JANUSENABLED && !is_testnet()) {
            if (length == JANUSRETARGETSTART) {
                override = true;
                nextTarget = TargetV2::initial();
            }
            if (length == JANUSV2RETARGETSTART) {
                override = true;
                nextTarget = TargetV2::initialv2();
            }
        }
        if (!override) {
            if (upperHeight != 1) {
                assert(latestRetargetHeight != 0);
                assert(latestRetargetTime != 0);
                assert(latestRetargetTime < timestamp);
                Height lowerHeight((upperHeight - 1).retarget_floor());
                assert(upperHeight - lowerHeight > 0);
                nextTarget.scale(timestamp - latestRetargetTime,
                    BLOCKTIME * (upperHeight - lowerHeight), newlength);
            }
        }
        latestRetargetHeight = upperHeight;
        latestRetargetTime = timestamp;
    }
}

auto HeaderVerifier::prepare_append(const std::optional<SignedSnapshot>& sp, HeaderView hv) const -> tl::expected<PreparedAppend, int32_t>
{
    auto hash { hv.hash() };
    NonzeroHeight appendHeight { height() + 1 };

    // Check header link
    if (hv.prevhash() != finalHash)
        return tl::make_unexpected(EHEADERLINK);

    // // Check version
    if (JANUSENABLED && (height().value() >= JANUSRETARGETSTART)) {
        if (hv.version() != 2) // For some reason some people mined with custom miner, and changed version ?!? so only stick to it after the mining alg change.
            return tl::make_unexpected(EBLOCKVERSION);
    }

    // Check difficulty
    if (hv.target(appendHeight, is_testnet()) != nextTarget)
        return tl::make_unexpected(EDIFFICULTY);

    // Check POW
    if (!hv.validPOW(hash, appendHeight, is_testnet())) {
        return tl::make_unexpected(EPOW);
    }

    // Check signed pin
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
    static_assert(JANUSRETARGETSTART > 1);
    if (length == 0) {
        if(is_testnet()){
            nextTarget = TargetV2::genesis_testnet();
        }else{
            nextTarget = TargetV1::genesis();
        } 
        latestRetargetHeight = Height(0);
        latestRetargetTime = 0;
    } else {
        latestRetargetHeight = upperHeight;
        auto upper { hc.get_header(upperHeight).value() };
        latestRetargetTime = upper.timestamp();

        // assign nextTarget
        bool override = false;
        if (JANUSENABLED && !is_testnet()) {
            if (length == JANUSRETARGETSTART) {
                override = true;
                nextTarget = TargetV2::initial();
            }
            if (length == JANUSV2RETARGETSTART) {
                override = true;
                nextTarget = TargetV2::initialv2();
            }
        }
        if (!override) {
            nextTarget = hc.get_header(length).value().target(length.nonzero_assert(), is_testnet());
            if (upperHeight != 1 && upperHeight == length) {
                Height lowerHeight = (upperHeight - 1).retarget_floor();
                assert(upperHeight - lowerHeight > 0);
                auto lower { hc.get_header(lowerHeight).value() };
                nextTarget.scale(latestRetargetTime - lower.timestamp(),
                    BLOCKTIME * (upperHeight - lowerHeight), length + 1);
            }
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
    worksum += checker.next_target();
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
