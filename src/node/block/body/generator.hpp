#pragma once
#include "block/body/container.hpp"
#include "block/body/primitives.hpp"
#include "general/byte_order.hpp"

class ChainDB;
BodyContainer generate_body(const ChainDB& db, NonzeroHeight height, const Payout& payout, const std::vector<TransferTxExchangeMessage>& payments);

