#pragma once
#include "block/chain/height.hpp"
class TransferView;
class TransferTxExchangeMessage;
void log_compressed(const TransferView& t, NonzeroHeight h);
void log_compressed(const TransferTxExchangeMessage& t, NonzeroHeight h);

