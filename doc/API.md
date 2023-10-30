# API DOCUMENTATION
## Configuration
API is accessed via the RPC endpoints of the node. The endpoints socket can be configured via the `--rpc` command line option:
```
JSON RPC endpoint options:
  -r, --rpc=IP:PORT          JSON RPC endpoint  (default=`127.0.0.1:3000')
```
For example invoke 
```
$./wart-node --rpc=0.0.0.0:3000
```
to start the node with RPC listening on all devices on port 3000.

**⚠ WARNING:** The RPC endpoint should not exposed to the internet, use appropriate firewall settings.

Below we assume the RPC socket is accessible at `localhost:3000`. On startup the node reports the RPC endpoint setting:
```
[info] RPC endpoint is 127.0.0.1:3000.
```

## Endpoint Overview 
You can see an HTML overview of all supported RPC endpoints by opening `localhost:3000` in your browser. Currently the following endpoints are supported:

METHOD| PATH | DESCRIPTION
------|------|------------
`POST`  |`/transaction/add`| Send transactions
`GET`   |`/transaction/mempool`| Show content of mempool
`GET`   |`/transaction/lookup/:txid`| Transaction lookup
`GET`   |`/chain/head`| Show info on chain head
`GET`   |`/chain/grid`| Show header grid (used for sync)
`GET`   |`/chain/block/:id/hash`| Show hash of specific block
`GET`   |`/chain/signed_snapshot`| Show chain snapshot
`GET`   |`/chain/block/:id/header`| Show header of specific block
`GET`   |`/chain/block/:id`| Show header and body of specific block
`GET`   |`/chain/mine/:address`| Generate data required for mining
`GET`   |`/chain/txcache`| Show transaction cache
`GET`   |`/chain/hashrate`| Show current hashrate
`POST`  |`/chain/append`| Append mined block
`GET`   |`/account/:account/balance`| Show balance of specific account
`GET`   |`/account/:account/history/:beforeTxIndex`| Show transaction history of specific account
`GET`   |`/peers/ip_count`| Show peer IPs
`GET`   |`/peers/banned`| Show banned peers
`GET`   |`/peers/unban`| Unban all peers
`GET`   |`/peers/offenses/:page`| Show offenses of peers
`GET`   |`/peers/connected`| Show info of connected peers
`GET`   |`/peers/endpoints`| Show known peer endpoints
`GET`   |`/peers/connect_timers`| Show timers used for reconnect
`GET `  |`/tools/encode16bit/from_e8/:feeE8`| Round raw 64 integer to closest 16 bit representation (for fee specification)
`GET`   |`/tools/encode16bit/from_string/:feestring`| Round coin amount string to closest 16 bit representation (for fee specification)

## Detailed Description

### `POST /transaction/add`
 Send transactions in JSON format:
 
 PARAMETER | TYPE | DETAILS
 ----------|------|--------
 pinHeight | unsigned 32 bit integer | Signature includes block hash at this height
 nonceId   | unsigned 32 bit integer | To avoid double spend, there can only be one transaction with a specific (pinHeight,nonceId) pair. The same nonceId can be used for different pinHeight values
 toAddr    | string of length 48| The address that coins shall be transferred to
 amountE8  | unsigned 64 bit integer | Amount of coins to send multiplied by 10^8. For example to send one coin this value must be 100000000.
 feeE8  | unsigned 64 bit integer | Amount of coins to spend on transaction fees multiplied by 10^8. For example to send one 0.00000001 coins this value must be 1. This value must be exactly representable value in a 16 bit encoding, see below.
 signature65| string of length 130 | hex-encoded 65 byte compact recoverable ECDSA signature in custom format, see below

#### How to specify the sender?
The sender's address is recovered from the recoverable ECDSA signature `signature65`. It is implicitly specified by creating a signature with the corresponding private key.

#### Signature generation
The following steps are required:
1. Call the `/chain/head` endpoint and extract the  `pinHash` and `pinHeight` fields.

2. Compute transaction hash. The transaction hash is the SHA256 hash of the following bytes:

BYTES | DESCRIPTION
------|------------
1 -32 | `pinHash` (hash of block at height `pinHeight`)
33-36 | `pinHeight` (`uint32_t` in network byte order)
37-40 | `nonceId` (`uint32_t` in network byte order)
41-43 | `reserved` (3 bytes containing 0)
44-51 | `feeE8` (`uint64_t` in network byte order)
52-71 | `toAddr` receiving address (20 bytes without the final 4 byte checksum)
72-79 | `amountE8` (`uint64_t` in network byte order)

3. Generate the secp256k1-ECDSA recoverable signature of the 32-byte transaction hash using the private key corresponding to the sender's address. The signature will have three properties:
- `r`: 32 byte coordinate parameter
- `s`: 32 byte coordinate parameter
- `recid`: 1 byte recovery id, it should automatically have one of the four values 0,1,2,3.

4. Concatenate the parameters to form the 65-byte compact recoverable signature with the following byte structure:

BYTES | DESCRIPTION
------|------------
1 -32 | `r` 
33-64 | `s` 
65    | `recid` 

Note that this is not the standard compact recoverable signature representation because in Warthog, the recoverable id is the last byte of the 65 byte signature and has no offset of 27.


#### Details on fees
Fees are not subtracted from the amount sent in the transaction. The sender spends both, `amountE8` and `feeE8`, `toAddr` receives `amountE8` and the miner of the block including this transaction gets `feeE8` as transaction fee.

For efficiency and compactness transaction fees are internally encoded as 2-byte floating-point numbers (16 bits), where the first 6 bits encode the exponent and the remaining 10 bits encode a 11 bit mantissa starting with an implicit 1. Of course not every 64 bit value can be encoded in 16 bits and only fee 64 bit values which are representable exactly in the 16 bits encoding are accepted. You can use the `/tools/encode16bit/from_e8/:feeE8` or `/tools/encode16bit/from_string/:feestring` endpoints to round an arbitrary 64-bit fee value to an accepted 64 bit value.


### `POST /transaction/add`
 Send transactions. At the moment only binary format is available. TODO: allow JSON format:

### `GET /transaction/mempool`
 Show content of mempool. Example output:
 ```json
{
 "code": 0,
 "data": {
  "data": []
 }
}
```

### `GET /transaction/lookup/:txid`
 Transaction lookup by transaction id. Example output of `/transaction/lookup/4b3bc48295742b71ff7c3b98ede5b652fafd16c67f0d2db6226e936a1cdbf0a5`:
 ```json
{
 "code": 0,
 "data": {
  "transaction": {
   "amount": "3.00000000",
   "amountE8": 300000000,
   "blockHeight": 376696,
   "confirmations": 8,
   "timestamp": 1695472249,
   "toAddress": "848b08b803e95640f8cb30af1b3166701b152b98c2cd70ee",
   "txHash": "4b3bc48295742b71ff7c3b98ede5b652fafd16c67f0d2db6226e936a1cdbf0a5",
   "type": "Reward",
   "utc": "2023-09-23 12:30:49 UTC"
  }
 }
} 
 ```
 
### `GET /chain/head`
 Show info on chain head. Example output:
 ```json
{
 "code": 0,
 "data": {
  "difficulty": 6139045663744.908,
  "hash": "0ba161f80aad860c41f879a4096fa5dcdd7eb3f1ccb23c8ad777010000000000",
  "height": 376696,
  "pinHash": "1c421a369caf7ce185b3c29d76ac8b99b75f7bdbbd6514c1e87a0b0000000000",
  "pinHeight": 376672,
  "worksum": 1.0027869358085505e+18,
  "worksumHex": "0x0000000000000000000000000000000000000000000000000dea9d67b64566a0"
 }
}
 ```
### `GET /chain/grid`
 Show hexadecimal header grid. This grid is used for chain sync to allow nodes spot points where chains diverge. Example output (truncated):
 ```json
{
 "code": 0,
 "data": [
  "19f65d40c6bc02a16378712bd7652eaebc925955e96c0262641314520000000021c88fbd99f0d3369f7f1ad981e11b8c6f032e13d4ef28311226ea5c8574c3fe3635b32500000001649f78127c04d23d",
  "0e43cf390ca986fabbed37a33386b4ad73723d51fab291d12013351f0000000021d1c6076e52222fb8c83fe33d55c610482cd272b7789646710510df3ba37a4f55b30e4e0000000164a221eb03577627",
  "2e92a976c5f7df4cfa26a37f534f79795d23342cd003fdaa000d28050000000021cab3fb524f9a8ca2cc60fbe97602c87079628007cedffdc35cfe9c2ee213fd065bfd890000000164a4c5ec4b03db81",
  "8473ff5f3f518d7c452cac1615bf8c9002bd8fbbbd55de46fe44093a0000000021bcc4fc5876f1e6cac619f2d8879440d1d8d12533366aae1d510b0545726e9001d3c19e0000000164a76310886953a6",
  "f0712262bd4e33de238d1e398af6b230a3d73a35b47f1594b0a937390000000021bbe95b117621d2cac808d78ec949629153d613b786ee6c36c7138648fac654ddda201f0000000164aa047487b3ac63",
  "46d35ed8925c8352ba39724f6300d722aa68647d384a1337cc1e2c0a0000000021cbd7eeb4de9855cf012681cc0136970ec2d0908f67e27e67a776b73938fd5bc028991e0000000164acb408c64731c8",
  "b519f66a8273a8e1a729b6a002fb559e6f9e8789acc4d3735650a00d0000000023d178c86190febff3340df034fe867fe19539d9a6c1715d9ce5775e5231db1c82d564ab0000000164af016a0fea6998",
  "ede75acb72ac56e888ffa70b05668e61425dd58859abaabe5e04fc040000000025d852ac6f8f2c63fda6d0de9dfa3cda237a56f3c5ffd3ac7108373e9d2890530f8f927b0000000164b174eec1703326",
  .
  .
  .
 ]
} 
 ```
 
### `GET /chain/signed_snapshot`
 Show chain snapshot. Example output
 ```json
{
 "code": 0,
 "data": {
  "header": {
   "difficulty": 5850035871080.048,
   "hash": "ac4f50ef90c0730075f4995d0b33691e9e7d69f84d2191c029371c0000000000",
   "merkleroot": "a4d44aff9ebd539f2b5e758fb76df22bfd13a2e4d894752c7892f9e9c3f3d969",
   "nonce": "0d8e8eab",
   "prevHash": "11f2b6860910f7b544e01fd5ed5419588c29e42b305553bd492a0d0000000000",
   "raw": "11f2b6860910f7b544e01fd5ed5419588c29e42b305553bd492a0d00000000002ac075d9a4d44aff9ebd539f2b5e758fb76df22bfd13a2e4d894752c7892f9e9c3f3d96900000001650bd0820d8e8eab",
   "target": "d975c02a",
   "timestamp": 1695273090,
   "utc": "2023-09-21 05:11:30 UTC",
   "version": "00000001"
  }
 }
}
 ```

### `GET /chain/block/:id/hash`
 Show hash of specific block. Example output of `/chain/block/366700/hash`
 ```json
{
 "code": 0,
 "data": {
  "hash": "ac4f50ef90c0730075f4995d0b33691e9e7d69f84d2191c029371c0000000000"
 }
}
```
 
### `GET /chain/block/:id/header`
 Show header of specific block. 

 Example output of `/chain/block/366700/header`
 ```json
 {
 "code": 0,
 "data": {
  "header": {
   "difficulty": 5850035871080.048,
   "hash": "ac4f50ef90c0730075f4995d0b33691e9e7d69f84d2191c029371c0000000000",
   "merkleroot": "a4d44aff9ebd539f2b5e758fb76df22bfd13a2e4d894752c7892f9e9c3f3d969",
   "nonce": "0d8e8eab",
   "prevHash": "11f2b6860910f7b544e01fd5ed5419588c29e42b305553bd492a0d0000000000",
   "raw": "11f2b6860910f7b544e01fd5ed5419588c29e42b305553bd492a0d00000000002ac075d9a4d44aff9ebd539f2b5e758fb76df22bfd13a2e4d894752c7892f9e9c3f3d96900000001650bd0820d8e8eab",
   "target": "d975c02a",
   "timestamp": 1695273090,
   "utc": "2023-09-21 05:11:30 UTC",
   "version": "00000001"
  }
 }
}
 ```
 

### `GET/chain/block/:id`
 Show header and body of specific block. 
 Example output of `/chain/block/366700`
 ```json
 {
 "code": 0,
 "data": {
  "body": {
   "rewards": [
    {
     "amount": "3.00000001",
     "amountE8": 300000001,
     "toAddress": "848b08b803e95640f8cb30af1b3166701b152b98c2cd70ee",
     "txHash": "660b6f0883e4b295d5a3920eea9c73636787b932efd00cacc6d3a9a89f540714"
    }
   ],
   "transfers": [
    {
     "amount": "5000.00000000",
     "amountE8": 500000000000,
     "fee": "0.00000001",
     "feeE8": 1,
     "fromAddress": "1711cadfe5a66a5f5f245fc78b2335f79da362178a72de2e",
     "nonceId": 131630562,
     "pinHeight": 366688,
     "toAddress": "8733d0e21bc791f44785f02753bb589b8423eb56cd938fbc",
     "txHash": "f364da997bf7a3c3bc8ead22041509228d6bdea39fcbcdc6be56030433d54219"
    }
   ]
  },
  "confirmations": 9970,
  "header": {
   "difficulty": 5850035871080.048,
   "hash": "ac4f50ef90c0730075f4995d0b33691e9e7d69f84d2191c029371c0000000000",
   "merkleroot": "a4d44aff9ebd539f2b5e758fb76df22bfd13a2e4d894752c7892f9e9c3f3d969",
   "nonce": "0d8e8eab",
   "prevHash": "11f2b6860910f7b544e01fd5ed5419588c29e42b305553bd492a0d0000000000",
   "raw": "11f2b6860910f7b544e01fd5ed5419588c29e42b305553bd492a0d00000000002ac075d9a4d44aff9ebd539f2b5e758fb76df22bfd13a2e4d894752c7892f9e9c3f3d96900000001650bd0820d8e8eab",
   "target": "d975c02a",
   "timestamp": 1695273090,
   "utc": "2023-09-21 05:11:30 UTC",
   "version": "00000001"
  },
  "height": 366700,
  "timestamp": 1695273090,
  "utc": "2023-09-21 05:11:30 UTC"
 }
}
```
### `GET /chain/mine/:address`
 Generate data required for mining. Example output of `/chain/mine/e4145cfe3e34f206956487c2a16b65a47f05fc347ef6e287`
```json
{
 "code": 0,
 "data": {
  "body": "0000000000000001e4145cfe3e34f206956487c2a16b65a47f05fc34000100000000000001800000000011e1a30000000000",
  "difficulty": 6139045663744.908,
  "header": "2ae4c944cae092bc9f3b93a6970e7b68d12256a641192170ef011600000000002ab7665b6a124de9367bf5c9c5775022c1cd60fbc839f6f51a671f0daed57b201ea8cb8100000001650edba500000000",
  "height": 376713
 }
}
```

### `GET /chain/txcache`
 Show transaction cache. Example output: 
 ```json
{
 "code": 0,
 "data": []
} 
 ```
 
### `GET /chain/hashrate`
 Show current hashrate
 ```json
{
 "code": 0,
 "data": {
  "last100BlocksEstimate": 296429051797
 }
} 
 ```
 
### `POST /chain/append`
 Append mined block. Miners must POST mined block they received from `GET /chain/mine/:address`.
 TODO: Detailed description
### `GET /account/:account/balance`
 Show balance of specific account. Example output:
 ```json
 {
 "code": 0,
 "data": {
  "accountId": 198,
  "balance": "5027.00000000",
  "balanceE8": 502700000000
 }
}
```
### `GET /account/:account/history/:beforeTxIndex`
 Show transaction history of specific account 
 Example output:
 ```json
{
 "code": 0,
 "data": {
  "balance": "5027.00000000",
  "balanceE8": 502700000000,
  "fromId": 69626,
  "perBlock": [
   {
    "confirmations": 9949,
    "height": 366700,
    "transactions": {
     "rewards": [],
     "transfers": [
      {
       "amount": "5000.00000000",
       "amountE8": 500000000000,
       "fee": "0.00000001",
       "feeE8": 1,
       "fromAddress": "1711cadfe5a66a5f5f245fc78b2335f79da362178a72de2e",
       "nonceId": 131630562,
       "pinHeight": 366688,
       "toAddress": "8733d0e21bc791f44785f02753bb589b8423eb56cd938fbc",
       "txHash": "f364da997bf7a3c3bc8ead22041509228d6bdea39fcbcdc6be56030433d54219"
      }
     ]
    }
   },
   {
    "confirmations": 298316,
    "height": 78333,
    "transactions": {
     "rewards": [
      {
       "amount": "3.00000000",
       "amountE8": 300000000,
       "toAddress": "8733d0e21bc791f44785f02753bb589b8423eb56cd938fbc",
       "txHash": "bc1b3a369c63884b013ff798ecb1d964685706d25bfaa8519d301779a6566b2a"
      }
     ],
     "transfers": []
    }
   },
   .
   .
   .
  ]
 }
}
```

### `GET /tools/encode16bit/from_e8/:feeE8`| Round raw 64 integer to closest 16 bit representation (for fee specification)
 Round raw fee integer representation (coin amount is this number divided by 10^8) to closest 16 bit representation. This is required for fee specification in the `/transaction/add` endpoint. Example output of `/tools/encode16bit/from_e8/5002`
```json
{
 "code": 0,
 "data": {
  "16bit": 12514,
  "originalAmount": "0.00005002",
  "originalE8": 5002,
  "roundedAmount": "0.00005000",
  "roundedE8": 5000
 }
}
```

### `GET /tools/encode16bit/from_string/:feestring`
 Round fee amount string to closest 16 bit representation. This is required for fee specification in the `/transaction/add` endpoint. Example output of `/tools/encode16bit/from_string/0.001`:

```json
{
 "code": 0,
 "data": {
  "16bit": 16922,
  "originalAmount": "0.00100000",
  "originalE8": 100000,
  "roundedAmount": "0.00099968",
  "roundedE8": 99968
 }
}
```
