#pragma once
#include "block/body/container.hpp"
#include "block/body/primitives.hpp"

class ChainDB;

VersionedBodyContainer generate_body(const ChainDB& db, NonzeroHeight height, const Address& miner, const std::vector<WartTransferMessage>& payments);
