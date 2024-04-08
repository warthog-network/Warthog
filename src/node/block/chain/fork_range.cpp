#include "fork_range.hpp"
#include "block/chain/binary_forksearch.hpp"
#include "block/chain/header_chain.hpp"
#include "block/chain/state.hpp"

ForkRange::ForkRange(const Headerchain& hc, const HashGrid& g, Batchslot begin)
{
    // OK
    auto [i, forked] = binary_forksearch(hc.hash_grid_view(), g, begin.index());
    Batchslot s(i);
    if (forked) {
        *this = { s.lower(), s.upper() };
    } else {
        *this = { s.lower() };
    }
}

void ForkRange::on_fork(NonzeroHeight forkHeight)
{ //OK
    if (forkHeight < l) {
        l = forkHeight;
        u = forkHeight;
    } else if (forkHeight <= u) {
        u = upper_open;
    }
}

void ForkRange::on_fork(NonzeroHeight forkHeight, const Descripted& theirs, const Headerchain& ours)
{
    on_fork(forkHeight);
    if (forked())
        return;
    assert(lower() <= forkHeight);
    grid_match(Batchslot(forkHeight), theirs.hash_grid(), ours);
}

bool ForkRange::detect_shrink(const Descripted& theirs, const Headerchain& ours)
{ //OK
    Height minlength = std::min(theirs.chain_length(), ours.length());
    if (l > minlength) {
        l = minlength.one_if_zero();
        u = upper_open;
        return true;
    }
    if (forked() && u > minlength) {
        u = upper_open;
        return true;
    }

    return false;
}
void ForkRange::on_append_or_shrink(const Descripted& theirs, const Headerchain& ours)
{ //OK
    if (detect_shrink(theirs, ours))
        return;
    on_append(theirs, ours);
}

void ForkRange::on_shrink(const Descripted& theirs, const Headerchain& ours)
{ //OK
    detect_shrink(theirs, ours);
}

void ForkRange::on_append(const Descripted& theirs, const Headerchain& ours)
{ // OK
    if (forked())
        return;
    grid_match(Batchslot(l), theirs.hash_grid(), ours);
}

void ForkRange::grid_match(Batchslot begin, const HashGrid& theirGrid, const Headerchain& ours)
{ // OK
    auto gv { ours.hash_grid_view() };
    Batchslot end1 = gv.slot_end();
    Batchslot end2 = theirGrid.slot_end();
    if (begin >= end1)
        return;
    if (begin >= end2)
        return;

    ForkRange r(ours, theirGrid, begin);
    on_match(r.lower() - 1);
    if (r.forked()) {
        on_mismatch(r.upper());
    }
}

ForkRange::Change ForkRange::on_match(Height matchHeight)
{ // OK
    if (matchHeight < l) {
    } else if (matchHeight < u) {
        l = (matchHeight + 1).nonzero_assert();
        return Change::lower();
    } else {
        // matchHeight is nonzero in this branch because l is nonzero.
        throw ChainError { EBADMATCH, matchHeight.nonzero_assert() }; 
    }
    return Change::none();
}

ForkRange::Change ForkRange::match(const Headerchain& hc, NonzeroHeight h, HeaderView hv)
{ // OK
    if (hc.length() < h)
        return Change::none();
    if (hc[h] == hv) {
        return on_match(h);
    } else {
        return on_mismatch(h);
    }
}

ForkRange::Change ForkRange::on_mismatch(NonzeroHeight mismatchHeight)
{ //OK
    if (mismatchHeight < l) {
        throw ChainError { EBADMISMATCH, mismatchHeight };
    } else if (mismatchHeight < u) {
        u = mismatchHeight;
        return Change::upper();
    } else {
    }
    return Change::none();
}
