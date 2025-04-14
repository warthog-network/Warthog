#pragma once
#include "expected.hpp"
#include "general/errors_forward.hpp"
template <typename T>
struct Result : public tl::expected<T, Error> {
    Result(tl::expected<T, Error> t)
        : tl::expected<T, Error>(std::move(t))
    {
    }
    Result(T t)
        : tl::expected<T, Error>(std::move(t))
    {
    }
    Result() // for Result<void> default constructor
        : tl::expected<T, Error>({})
    {
    }
    Result(Error e)
        : tl::expected<T, Error>(tl::make_unexpected(e))
    {
    }
};
