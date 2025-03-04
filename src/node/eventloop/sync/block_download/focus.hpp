#pragma once
#include "block/block.hpp"
#include "block/chain/header_chain.hpp"
#include "block/chain/height.hpp"
#include "eventloop/types/conref_declaration.hpp"
#include "eventloop/types/peer_requests.hpp"

// #include "conr
namespace BlockDownload {

class Downloader;
struct FocusNode;
struct BlockSlot {
    explicit BlockSlot(uint32_t i)
        : i(i)
    {
    }
    explicit BlockSlot(Height h)
        : BlockSlot((h.value() - 1) / BLOCKBATCHSIZE)
    {
    }
    Height height_offset() const
    {
        return Height(i * BLOCKBATCHSIZE);
    }
    NonzeroHeight lower_height() const
    {
        return i * BLOCKBATCHSIZE + 1;
    }
    Height upper_height() const
    {
        return Height((i + 1) * BLOCKBATCHSIZE);
    }
    BlockSlot operator+(uint32_t add) const
    {
        return BlockSlot(i + add);
    }
    uint32_t operator-(BlockSlot s) const
    {
        return i - s.i;
    }
    BlockSlot& operator++()
    {
        i += 1;
        return *this;
    }
    size_t index() { return i; }

    friend bool operator==(const BlockSlot&, const BlockSlot&) = default;

    auto operator<=>(const BlockSlot& h2) const = default;

private:
    uint32_t i;
};

struct FocusNode {
    std::vector<ParsedBlock> blocks;
    bool activeRequest() const { return c.has_value(); }
    void register_downloader(Conref);

    std::optional<Conref> c; // connection downloading this block batch
    std::vector<Conref> refs;
};
class Focus {
    public:
    using FocusMap = std::map<BlockSlot, FocusNode>;
    Focus(const Downloader& dl, size_t windowWidth)
        : downloader(dl)
        , width(windowWidth) {};
    bool has_data();

    NonzeroHeight height_begin();
    void fork(NonzeroHeight);
    std::vector<ParsedBlock> pop_data();
    auto map_end() { return map.end(); }
    void clear(); // precondition: reset all connections focusIter
    void erase(Conref cr);
    void set_offset(Height);
    void set_slot_blocks(std::vector<ParsedBlock>&& blocks);

    struct FocusSlot {
        FocusMap::iterator iter;
        BlockRange r;
        Focus& focus;
        Blockrequest link_request(Conref cr);
    };
    struct EndIterator {
    };
    class Iterator {
        friend class Focus;

    public:
        Iterator& operator++()
        {
            i += 1;
            return *this;
        }
        std::optional<FocusSlot> operator*()
        {
            auto slot { downloadSlot + i };
            auto iter { focus.map.try_emplace(slot).first };
            size_t presentBlocks { iter->second.blocks.size() };
            auto upper { std::min(slot.upper_height(), hc.length()) };
            NonzeroHeight lower { std::max(slot.lower_height(), focus.height_begin()) + presentBlocks };
            assert(lower <= upper + 1);
            if (lower == upper + 1) {
                return {};
            }
            return FocusSlot {
                .iter { iter },
                .r { lower, upper.nonzero_assert() },
                .focus { focus }
            };
        }
        bool operator==(EndIterator)
        {
            return i >= bound;
        }

    private:
        Iterator(Focus& focus, const Headerchain& hc)
            : focus(focus)
            , hc(hc)
            , downloadSlot(focus.height_begin())
            , maxSlot(hc.length())
            , bound(std::min(focus.width, size_t(maxSlot - downloadSlot + 1)))
        {
            assert(downloadSlot <= maxSlot);
        };
        Focus& focus;
        const Headerchain& hc;
        size_t i { 0 };
        const BlockSlot downloadSlot;
        const BlockSlot maxSlot;
        size_t bound;
    };
    Iterator begin();
    EndIterator end() { return {}; }

private:
    void advance(Height newOffset);
    void map_erase(FocusMap::iterator);
    const Headerchain& headers();

private:
    const Downloader& downloader;
    size_t width;
    FocusMap map;
    Height downloadLength { 0 };
};
}
