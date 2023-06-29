#pragma once
#include "crypto/hash.hpp"
#include "general/funds.hpp"
#include <array>
#include <string>
#include <vector>
struct PinHeight;
Funds get_balance(const std::string& account);
int32_t send_transaction(const std::vector<uint8_t>& txdata, std::string* error);
std::pair<PinHeight, Hash> get_pin();

