#include "focus.hpp"
#include "block_download.hpp"
#include "spdlog/spdlog.h"

namespace BlockDownload {
struct __attribute__((visibility("hidden"))) Node {
private:
public:
    Height batch_lower() const
    {
        return iter->first.lower_height();
    }
    Height batch_upper() const
    {
        return iter->first.upper_height();
    }
    auto& blocks() const
    {
        return iter->second.blockBodies;
    }
    bool covers_next(Height downloadLength) const
    {
        return downloadLength + 1 >= batch_lower() && downloadLength + 1 <= batch_upper() && blocks().size() > 0;
    }
    bool complete()
    {
        return blocks().size() == BLOCKBATCHSIZE;
    }
    const FocusMap::iterator& iter;
};

auto Focus::begin() -> Iterator
{
    return { *this, downloader.headers() };
}

const Headerchain& Focus::headers()
{
    return downloader.headers();
}

BlockRequest Focus::FocusSlot::link_request(Conref cr)
{
    assert(data(cr).focusIter == focus.map.end());

    // establish link
    data(cr).focusIter = iter;
    iter->second.c = cr;
    iter->second.refs.push_back(cr);

    // craft block request
    auto& descripted = data(cr).descripted();
    return BlockRequest(descripted, r, focus.headers().hash_at(r.last()));
}

bool Focus::has_data()
{
    if (map.size() > 0) {
        auto iter = map.begin();
        Node node { .iter = iter };
        return node.covers_next(downloadLength);
    }
    return false;
}

std::vector<Block> Focus::pop_data()
{
    assert(has_data());
    std::vector<Block> out;
    for (auto iter = map.begin(); iter != map.end();) {
        Node node { .iter = iter };
        if (!node.covers_next(downloadLength))
            break;
        auto& from = node.blocks();
        out.reserve(out.size() + from.size());
        for (size_t j = 0; j < from.size(); ++j) {
            ++downloadLength;
            auto h { downloadLength.nonzero_assert() };
            out.push_back(Block {
                .height = h,
                .header = headers()[h],
                .body = std::move(from[j]) });
        }
        if (downloadLength < node.batch_upper()) {
            from.clear();
            break;
        }
        assert(downloadLength == node.batch_upper());
        map_erase(iter++);
    }
    assert(out.size() > 0);
    return out;
}

NonzeroHeight Focus::height_begin()
{
    return (downloadLength + 1).nonzero_assert();
}

void Focus::map_erase(FocusMap::iterator iter)
{
    for (auto c : iter->second.refs) {
        auto& focusIter = data(c).focusIter;
        assert(focusIter == iter);
        focusIter = map.end();
    }
    map.erase(iter);
}

void Focus::fork(NonzeroHeight fh)
{
    BlockSlot bs(fh);
    auto iter = map.lower_bound(bs);
    if (iter != map.end()) {

        // partially shrink first block batch affected by fork
        if (iter->first == bs) {
            Height batchBegin { bs.lower_height() };
            assert(batchBegin <= fh);
            auto& b = iter->second.blockBodies;
            size_t newsize = fh - batchBegin;
            if (b.size() > newsize)
                b.erase(b.begin() + newsize, b.end());
            ++iter;
        }

        // delete higher block batch nodes
        while (iter != map.end())
            map_erase(iter++);
    }
}

void Focus::clear()
{
    for (auto& [f, node] : map) {
        for (auto& c : node.refs)
            data(c).focusIter = map.end();
    }
    map.clear();
    downloadLength = Height(0);
}

void Focus::erase(Conref cr)
{
    auto& focusIter { data(cr).focusIter };
    if (focusIter != map.end()) {
        // unlink from focusNode
        auto& focusNode { focusIter->second };
        if (focusNode.c == cr) {
            focusNode.c.reset();
        }
        std::erase(focusNode.refs, cr);

        // unlink from connection data
        focusIter = map.end();
    }
}

void Focus::advance(Height newOffset)
{
    assert(newOffset >= downloadLength);
    BlockSlot bs(newOffset + 1);
    auto iter = map.begin();
    while (iter != map.end()) {
        assert(iter->first >= BlockSlot(downloadLength + 1));
        Node node { .iter = iter };
        if (iter->first > bs)
            break;
        else if (iter->first == bs) {
            assert(newOffset + 1 >= node.batch_lower());
            assert(newOffset + 1 <= node.batch_upper());
            size_t nErase = newOffset + 1 - node.batch_lower();
            auto& bs = node.blocks();
            if (nErase > bs.size()) {
                bs.clear();
                map_erase(iter);
            } else {
                bs.erase(bs.begin(), bs.begin() + nErase);
            }
            break;
        } else { // (iter->first < bs)
            map_erase(iter++);
        }
    };

    downloadLength = newOffset;
}

void Focus::set_offset(Height newOffset)
{
    if (newOffset >= downloadLength) {
        advance(newOffset);
        return;
    } else {
        auto tmp { downloadLength };
        downloadLength = newOffset;
        if (map.size() == 0)
            return;
        assert(map.begin()->first >= BlockSlot(tmp + 1));

        BlockSlot bs(newOffset + 1);
        auto iter { map.lower_bound(bs) };
        if (iter->first == bs) {
            // only partial request in this block batch because
            // newOffset+1 < downloadLength+1 and both height are within the same batch
            // -> need to erase it
            map_erase(iter);
        }
    }
}

void Focus::set_blocks(BlockSlot slot, Height reqBegin, std::vector<BodyContainer>&& blocks)
{
    auto [iter, created] { map.try_emplace(slot) };
    FocusNode& fn { iter->second };
    if (created) {
        if (reqBegin != slot.lower_height()) {
            spdlog::debug("Discarding block batch, slotStart: {}, lowerHeight: {}",
                slot.lower_height().value(), reqBegin.value());
            return;
        }
        fn.blockBodies = std::move(blocks);
    } else { // already present
        auto& blockBodies = fn.blockBodies;
        auto missingStart = std::max(slot.lower_height(), (downloadLength + 1).nonzero_assert()) + blockBodies.size();
        if (missingStart < reqBegin) {
            spdlog::debug("Discarding block batch, missingStart: {}, lowerHeight: {}",
                missingStart.value(), reqBegin.value());
            return;
        }

        for (size_t i = missingStart - reqBegin; i < blocks.size(); ++i) {
            blockBodies.push_back(std::move(blocks[i]));
        }
        assert(blockBodies.size() <= BLOCKBATCHSIZE);
    }
}
}
