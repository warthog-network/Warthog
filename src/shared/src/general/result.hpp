#pragma once
#include "general/errors.hpp"
#include "wrt/expected.hpp"
#include "wrt/optional.hpp"
template <typename T>
struct Result : public wrt::expected<T, Error> {
    Result(wrt::expected<T, Error> t)
        : wrt::expected<T, Error>(std::move(t))
    {
    }
    Result(wrt::optional<T> t)
        : Result(
              [&]() -> Result {
                  if (t) {
                      return Result(std::move(*t));
                  } else {
                      return Result(Error(ENOTFOUND));
                  }
              }())
    {
    }
    Result(T t)
        : wrt::expected<T, Error>(std::move(t))
    {
    }
    Result(Error e)
        : wrt::expected<T, Error>(tl::make_unexpected(e))
    {
    }
};

template <>
struct Result<void> : public wrt::expected<void, Error> {
    Result(wrt::expected<void, Error> t)
        : wrt::expected<void, Error>(std::move(t))
    {
    }
    Result() // for Result<void> default constructor
        : wrt::expected<void, Error>({})
    {
    }
    Result(Error e)
        : wrt::expected<void, Error>(tl::make_unexpected(e))
    {
    }
};
