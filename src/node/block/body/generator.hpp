#pragma once
#include "block/body/container.hpp"
#include "block/body/primitives.hpp"
#include "chainserver/db/types_fwd.hpp"


VersionedBodyContainer generate_body(const ChainDB& db, NonzeroHeight height, const Address& miner, const std::vector<WartTransferMessage>& payments);
