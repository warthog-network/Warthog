#pragma once
#include "expected.hpp"
#include <cstdint>
#include <string>
struct Error { // error class for exceptions
    constexpr Error(int32_t e = 0)
        : code(e) { };
    const char* strerror() const;
    const char* err_name() const;
    std::string format() const;
    uint32_t bantime() const;
    bool triggers_ban() const;
    bool is_error() const { return code != 0; }
    operator bool() const { return is_error(); }
    operator int() const { return code; }
    int32_t code;
};

class NonzeroHeight;
struct ChainError : public Error {
    ChainError(Error e, NonzeroHeight height);
    NonzeroHeight height() const;

private:
    uint32_t h;
};
