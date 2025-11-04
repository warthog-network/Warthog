#pragma once
#include "wrt/expected.hpp"
#include "general/errors_forward.hpp"
template <typename T>
struct Result : public wrt::expected<T, Error> {
    Result(wrt::expected<T, Error> t)
        : wrt::expected<T, Error>(std::move(t))
    {
    }
    Result(T t)
        : wrt::expected<T, Error>(std::move(t))
    {
    }
    Result() // for Result<void> default constructor
        : wrt::expected<T, Error>({})
    {
    }
    Result(Error e)
        : wrt::expected<T, Error>(tl::make_unexpected(e))
    {
    }
};
