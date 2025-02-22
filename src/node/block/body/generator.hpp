#pragma once
#include "block/body/container.hpp"
#include "block/body/primitives.hpp"

class ChainDB;

std::pair<BodyContainer, BlockVersion> generate_body(const ChainDB& db, NonzeroHeight height, const Address& miner, const std::vector<TransferTxExchangeMessage>& payments);
