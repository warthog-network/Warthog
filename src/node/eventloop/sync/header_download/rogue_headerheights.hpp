#pragma once
#include "block/chain/height_header_work.hpp"
#include "block/header/header.hpp"

class HeaderchainSkeleton;
class RogueHeaders {
public:
    bool add(const RogueHeaderData&);
    size_t size() const { return n; }
    void clear()
    {
        m.clear();
        n = 0;
    }
    size_t prune(const Worksum& minWork);
    [[nodiscard]] std::optional<ChainError> find_rogue_in(const HeaderchainSkeleton&) const;

private:
    static constexpr size_t MAXSIZE = 100;

    struct NodeValue {
        Header header;
        Worksum worksum;
        Error error;
    };
    using vector_t = std::vector<NodeValue>;
    struct Node {
        bool insert(NodeValue v);
        size_t prune(const Worksum& ws);
        size_t size() const { return vec.size(); }
        auto begin() const { return vec.begin(); }
        auto end() const { return vec.end(); }
        vector_t vec;
    };

    using map_t = std::map<NonzeroHeight, Node>;
    struct const_reverse_sentinel {
    };

    class const_element_view {
        using mv_t = map_t::value_type;

    public:
        const_element_view(const mv_t& mvr, const NodeValue& node)
            : mvr(mvr)
            , nv(node)
        {
        }
        const Header& header() { return nv.header; }
        const Worksum& worksum() { return nv.worksum; }
        Error error() { return nv.error; }
        NonzeroHeight height() { return mvr.first; }

    private:
        const mv_t& mvr;
        const NodeValue& nv;
    };
    struct const_reverse_iterator {
        friend class RogueHeaders;

    private:
        const RogueHeaders& ref;
        map_t::const_reverse_iterator mr_it;
        std::optional<vector_t::const_iterator> it;

    private:
        const_reverse_iterator(const RogueHeaders& rrh)
            : ref(rrh)
            , mr_it(rrh.m.rbegin())
        {
            if (mr_it != rrh.m.rend()) {
                it = mr_it->second.vec.begin();
            }
        }

    public:
        bool operator==(const const_reverse_sentinel&)
        {
            return mr_it == ref.m.rend();
        }
        const_reverse_iterator operator++()
        {
            if ((++(*it) == mr_it->second.vec.end())
                && (++mr_it != ref.m.rend())) {
                assert(mr_it->second.vec.size() != 0);
                it = mr_it->second.vec.begin();
            }
            return *this;
        }
        const_element_view operator*() const
        {
            const NodeValue& nv { **it };
            return { *mr_it, nv };
        }
    };
    const_reverse_iterator rbegin() const
    {
        return { *this };
    }
    const_reverse_sentinel rend() const
    {
        return {};
    }

    map_t m;
    size_t n { 0 };
};
