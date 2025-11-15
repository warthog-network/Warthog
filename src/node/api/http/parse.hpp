#pragma once
#include "communication/create_transaction.hpp"
#include "communication/mining_task.hpp"
BlockWorker parse_block_worker(const std::vector<uint8_t>& s);
TransactionCreate parse_transaction_create(const std::vector<uint8_t>& s);
