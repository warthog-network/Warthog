#pragma once

#include <optional>
#include <variant>
namespace wrt{ // wart tools namesapce 
template <typename... Ts>
struct variant : public std::variant<Ts...> {
    using std::variant<Ts...>::variant;
    template <typename... Rs>
    struct Overload : Rs... {
        using Rs::operator()...;
    };
    template <typename... Rs>
    Overload(Rs...) -> Overload<Rs...>;

    template <typename... U>
    auto visit_overload(U&&... u) const
    {
        return visit(Overload { std::forward<U>(u)... });
    }
    auto visit(auto lambda) const
    {
        return std::visit(lambda, *this);
    }
    template <typename T>
    bool holds() const { return std::holds_alternative<T>(*this); }
    template <typename T>
    auto& get() const { return std::get<T>(*this); }

    template <typename T>
    auto& get() { return std::get<T>(*this); }
};

template <typename Id, typename Alt>
struct alternative {
public:
    alternative(Id id)
        : v(id)
    {
    }
    alternative(Alt a)
        : v(std::move(a))
    {
    }

    auto visit(auto lambda)
    {
        return v.visit(lambda);
    }
    auto visit(auto lambda) const
    {
        return v.visit(lambda);
    }
    std::optional<Id> map_alternative(auto lambda) const
    {
        return v.visit_overload(
            [&](Id id) -> std::optional<Id> { return id; },
            [&](const Alt& a) -> std::optional<Id> { return lambda(a); });
    }

private:
    wrt::variant<Id, Alt> v;
};
}
