#include "general/reader.hpp"
#include "general/writer.hpp"
#include "variant.hpp"
namespace wrt {
template <typename gen_parse_exception, typename... Ts>
struct indicator_variant : public wrt::variant<Ts...> {
private:
    static constexpr auto Max(auto const& value, auto const&... args)
    {
        if constexpr (sizeof...(args) == 0u) // Single argument case!
            return value;
        else {
            const auto max = Max(args...);
            return value > max ? value : max;
        }
    }

public:
    static constexpr const size_t max_byte_size = Max(Ts::byte_size()...);
    [[nodiscard]] static std::optional<indicator_variant> try_parse(Reader& r)
    {
        std::optional<indicator_variant> o;
        auto i { r.uint8() };
        if (([&] {
                if (i == Ts::INDICATOR) {
                    o.template emplace<Ts>(r);
                    return true;
                }
                return false;
            }() || ...))
            return o;
        return {};
    }
    static constexpr bool is_ascending()
    {
        constexpr uint8_t values[] = { Ts::INDICATOR... };
        for (size_t i = 1; i < sizeof...(Ts); ++i) {
            if (!(values[i - 1] < values[i]))
                return false;
        }
        return true;
    }
    static_assert(is_ascending()); // indicators must be in ascending order to ensure they are all different

    [[nodiscard]] static indicator_variant parse_throw(Reader& r)
    {
        if (auto o { try_parse(r) })
            return *o;
        throw gen_parse_exception();
    }

    using wrt::variant<Ts...>::variant;
    using parent_t1 = wrt::variant<Ts...>;
    indicator_variant(wrt::variant<Ts...> v)
        : wrt::variant<Ts...>(std::move(v))
    {
    }
    indicator_variant(Reader& r)
        : indicator_variant(parse_throw(r))
    {
    }
    size_t byte_size() const
    {
        return 1 + this->visit([](auto& t) { return t.byte_size(); });
    }
    void serialize(Serializer auto& s) const
    {
        this->visit([&](auto& v) {
            s << v.INDICATOR << v;
        });
    }
    std::vector<uint8_t> to_bytes() const
    {
        std::vector<uint8_t> out;
        out.resize(byte_size());
        Writer w(out);
        w << *this;
        assert(w.remaining() == 0);
        return out;
    }
};

}
