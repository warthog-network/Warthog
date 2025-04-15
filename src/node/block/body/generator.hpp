#pragma once
#include "block/body/container.hpp"
#include "block/body/primitives.hpp"

class ChainDB;

BodyContainerV3 generate_body(const ChainDB& db, NonzeroHeight height, const Address& miner, const std::vector<WartTransferMessage>& payments);
