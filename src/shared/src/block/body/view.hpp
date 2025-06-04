#pragma once

// #include "block/body/elements_fwd.hpp"
// #include "block/chain/height.hpp"
// #include "block/header/header.hpp"
// #include "crypto/hash.hpp"
// #include "defi/token/id.hpp"
// #include "view_fwd.hpp"
// #include <span>

// class AddressView;
// namespace block {
// namespace body {
//
// template <typename T>
// struct Indexed : public T {
//     size_t index;
//     Indexed(T v, size_t index)
//         : T(std::move(v))
//         , index(index)
//     {
//     }
// };
//
// template <typename TransactionView>
// struct Section {
//     using view_t = TransactionView;
//     using elem_t = Indexed<view_t>;
//     Section()
//         : n(0)
//         , offset(0)
//     {
//     }
//     Section(size_t n, Reader& r);
//     elem_t at(const uint8_t* blockData, size_t i) const
//     {
//         assert(i <= n);
//         view_t v { blockData + offset + TransactionView::size() * n };
//         return { v, i };
//     }
//     auto size() const { return n; }
//     size_t n;
//     size_t offset;
// };
//
// template <typename SectionType>
// struct SectionRange {
//     using elem_t = SectionType::elem_t;
//     SectionRange(const uint8_t* blockData, const SectionType& s)
//         : data(blockData)
//         , section(s)
//     {
//     }
//
//     void for_each(auto lambda)
//     {
//         for (size_t i = 0; i < size(); ++i)
//             lambda(section.at(data, i));
//     }
//     struct sentinel {
//     };
//     struct const_iterator {
//         friend bool operator==(const const_iterator& it, const sentinel&)
//         {
//             return it.i >= it.sr.section.n;
//         }
//         elem_t operator*() const
//         {
//             return sr.section.at(sr.data, i);
//         }
//         const_iterator& operator++()
//         {
//             i += 1;
//             return *this;
//         }
//         const SectionRange& sr;
//         size_t i { 0 };
//     };
//     const_iterator begin() const
//     {
//         return { *this, 0 };
//     };
//     sentinel end() const { return {}; }
//     size_t size() { return section.n; }
//
//     friend struct const_iterator;
//
// private:
//     const uint8_t* data;
//     const SectionType& section;
// };
//
// struct TokenSection {
//     TokenId id;
//     Section<Transfer> transfers;
//     Section<Order> orders;
//     Section<LiquidityAdd> liquidityAdd;
//     Section<LiquidityRemove> liquidityRemove;
//     Section<Cancelation> cancelations;
// };
//
// class Structure {
// public:
// private:
//     friend TokenSectionInteractor;
//     friend TokensRange;
//     friend BodyView;
//     Structure() { };
//
// public:
//     static Structure parse_throw(std::span<const uint8_t> s, NonzeroHeight h, BlockVersion version);
//
// private:
//     Section<AddressView> addresses;
//     view::Reward reward { nullptr };
//     Section<view::WartTransfer> wartTransfers;
//     std::vector<TokenSection> tokens;
//     Section<view::TokenCreation> tokenCreations;
// };
//
// class BodyView {
//     friend TokensRange;
//     friend TokenSectionInteractor;
//     auto section_range(auto& section) const
//     {
//         return SectionRange(data(), section);
//     }
//     void for_each(auto& section, auto lambda) const
//     {
//         section_range(section).for_each(lambda);
//     }
//
// public:
//     BodyView(const Container& bodyContainer, const Structure& structure)
//         : bodyContainer(bodyContainer)
//         , structure(structure) { };
//     std::vector<Hash> merkle_leaves() const;
//     Hash merkle_root(Height h) const;
//     std::vector<uint8_t> merkle_prefix() const;
//     size_t size() const { return bodyContainer.size(); }
//     const uint8_t* data() const { return bodyContainer.data().data(); }
//
//     auto wart_transfers() const { return section_range(structure.wartTransfers); }
//     auto addresses() const { return section_range(structure.addresses); }
//     TokensRange tokens() const;
//     auto token_creations() const { return section_range(structure.tokenCreations); };
//     inline auto foreach_token(auto lambda) const;
//     auto reward() const { return structure.reward; };
//     Funds_uint64 fee_sum_assert() const;
//     AddressView get_address(size_t i) const;
//
// private:
//     const Container& bodyContainer;
//     const Structure& structure;
// };
//
// class TokenSectionInteractor {
//     friend TokensRange;
//     auto section_range(auto& section) const { return bodyView.section_range(section); }
//
// public:
//     auto id() const { return token.id; }
//     auto transfers() const { return section_range(token.transfers); }
//     auto orders() const { return section_range(token.orders); }
//     auto liquidityAdd() const { return section_range(token.liquidityAdd); }
//     auto liquidityRemove() const { return section_range(token.liquidityRemove); }
//     auto cancelations() const { return section_range(token.cancelations); }
//
//     void visit(auto lambda)
//     {
//         for (auto t : transfers())
//             lambda(t);
//         for (auto t : orders())
//             lambda(t);
//         for (auto t : liquidityAdd())
//             lambda(t);
//         for (auto t : liquidityRemove())
//             lambda(t);
//         for (auto t : cancelations())
//             lambda(t);
//     }
//
// private:
//     TokenSectionInteractor(const BodyView& bodyView, const TokenSection& tokenSection)
//         : bodyView(bodyView)
//         , token(tokenSection)
//     {
//     }
//     const BodyView& bodyView;
//     const TokenSection& token;
// };
//
// class TokensRange {
// public:
//     TokensRange(const BodyView& body)
//         : body(body)
//     {
//     }
//     class sentinel { };
//     class const_iterator {
//     private:
//         const TokensRange& r;
//         size_t i { 0 };
//
//         friend TokensRange;
//         const_iterator(const TokensRange& tr, size_t i)
//             : r(tr)
//             , i(i)
//         {
//         }
//
//     public:
//         TokenSectionInteractor operator*() const
//         {
//             return { r.body, r.body.structure.tokens[i] };
//         };
//         friend bool operator==(const const_iterator& it, const sentinel&)
//         {
//             return it.r.body.structure.tokens.size() <= it.i;
//         }
//         const_iterator& operator++()
//         {
//             i += 1;
//             return *this;
//         }
//     };
//     auto begin() const { return const_iterator(*this, 0); }
//     auto end() const { return sentinel(); }
//
// private:
//     const BodyView& body;
// };
//
// inline TokensRange BodyView::tokens() const { return *this; }
//
// inline Funds_uint64 BodyView::fee_sum_assert() const
// {
//     Wart sum { Wart::zero() };
//     sum.subtract_assert(sum);
//     for (auto t : wart_transfers())
//         sum.add_assert(t.compact_fee_assert().uncompact());
//     return sum;
// }
//
// }
// }
