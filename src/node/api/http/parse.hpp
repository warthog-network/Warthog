#pragma once
#include "communication/create_payment.hpp"
#include "communication/mining_task.hpp"
BlockWorker parse_block_worker(const std::vector<uint8_t>& s);
WartTransferCreate parse_payment_create(const std::vector<uint8_t>& s);
// Funds_uint64 parse_funds(const std::vector<uint8_t>& s);
