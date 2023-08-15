#pragma once
#include <cstddef>
#include <cstdint>
#include <tuple>
#include "block/header/header.hpp"

std::tuple<bool, bool, uint32_t> mine(Header& header, uint32_t stop, uint32_t tries);
