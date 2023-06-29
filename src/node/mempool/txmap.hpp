#pragma once

#include "block/body/transaction_id.hpp"
#include "entry.hpp"
#include <map>

class HashView;
class TransferTxExchangeMessageView;
namespace mempool {
using Txmap = std::map<TransactionId, EntryValue, std::less<>>;
}
