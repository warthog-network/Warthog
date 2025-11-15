#pragma once
class WartTransferCreate;
class TokenTransferCreate;
class OrderCreate;
class LiquidityDepositCreate;
class LiquidityWithdrawalCreate;
class CancelationCreate;
class AssetCreationCreate;
template <typename... Ts>
struct TransactionCreateCombine;
using TransactionCreate = TransactionCreateCombine<WartTransferCreate, TokenTransferCreate, OrderCreate, LiquidityDepositCreate, LiquidityWithdrawalCreate, CancelationCreate, AssetCreationCreate>;
