# Python Integration Guide
In this guide we demonstrate how to handle wallets and send transactions with Python3. We use the `pycryptodome` and `pycoin` Python packages.

## Handling wallets

Below we demonstrate how to
- generate a new private key,
- load a private key from hexadecimal encoding, 
- derive a public key from a private key,
- derive a raw Warthog address form a public key and
- extend the raw Warthog address by its checksum to obtain an ordinary Warthog address. 
```python
from ecdsa import SigningKey, SECP256k1

##############################
# Handling wallets
##############################

# generate private key
pk = SigningKey.generate(curve=SECP256k1)

# alternatively read private key
pkhex= '966a71a98bb5d13e9116c0dffa3f1a7877e45c6f563897b96cfd5c59bf0803e0'
pk = SigningKey.from_string(bytes.fromhex(pkhex),curve=SECP256k1)

# print private key
print("private_key:")
print(pk.to_string().hex())

# derive public key
pubkey = pk.get_verifying_key().to_string('compressed')

# print public key
print("public key:")
print(pubkey.hex())

# convert public key to raw addresss
from Crypto.Hash import RIPEMD160, SHA256
sha = SHA256.SHA256Hash(pubkey).digest()
addr_raw = RIPEMD160.RIPEMD160Hash(sha).digest()
addr_raw.hex()

# generate address by appending checksum
addr_hash = SHA256.SHA256Hash(addr_raw).digest()
checksum = addr_hash[0:4]
addr = addr_raw + checksum

# print address
print("address:")
print(addr.hex())
```

## Generating, signing and sending a transfer transaction

In Warthog transactions are pinned to a specific block the blockchain. The height of this block is its `pinHeight`, its hash the `pinHash`. This information has to be retrieved from the node in order to start generating a transaction, a transaction with a `pinHeight` that is too old be discarded. 
This serves two purposes
1. Users can be sure that transactions will not be pending forever.
2. By choosing a particular `pinHeight` users can actively control the time to live (TTL) of a pending transaction before it is discarded.

Not all height values are valid `pinHeights`, instead they must be a multiple of 32. For convenience the `/chain/head` endpoint provides the latest `pinHeight` value together with the corresponding block hash `pinHash`. In our code sample below we will use this endpoint.

In addition we demonstrate how to create the bytes to sign, how a valid sekp256k1 recoverable signature in custom format is generated and how to send the transaction to the Warthog node via a HTTP POST request. For reference read the [API guide](API.md). The below code uses the private key `pk` variable from the above code snippet.

```python
##############################
# Generate a signed transaction
##############################
import requests
import json
baseurl="http://localhost:3000"

# get pinHash and pinHeight from warthog node
head_raw = requests.get(baseurl+'/chain/head').content
head = json.loads(head_raw)
pinHash =  head["data"]["pinHash"]
pinHeight = head["data"]["pinHeight"]


# send parameters
nonceId = 0 # 32 bit number, unique per pinHash and pinHeight
toAddr = '0000000000000000000000000000000000000000de47c9b2' # burn destination address
amountE8 = 100000000 # 1 WART, this must be an integer, coin amount * 10E8


# round fee from WART amount
rawFee = "0.00009999" # this needs to be rounded, WARNING: NO SCIENTIFIC NOTATION
result = requests.get(baseurl+'/tools/encode16bit/from_string/'+rawFee).content
encode16bit_result = json.loads(result)
feeE8 = encode16bit_result["data"]["roundedE8"] # 9992


# alternative: round fee from E8 amount
rawFeeE8 = "9999" # this needs to be rounded
result = requests.get(baseurl+'/tools/encode16bit/from_e8/'+rawFeeE8).content
encode16bit_result = json.loads(result)
feeE8 = encode16bit_result["data"]["roundedE8"] # 9992


# generate bytes to sign
to_sign =\
bytes.fromhex(pinHash)+\
pinHeight.to_bytes(4, byteorder='big') +\
nonceId.to_bytes(4, byteorder='big') +\
b'\x00\x00\x00'+\
feeE8.to_bytes(8, byteorder='big')+\
bytes.fromhex(toAddr)[0:20]+\
amountE8.to_bytes(8, byteorder='big')


# create signature
from pycoin.ecdsa.secp256k1 import secp256k1_generator
from hashlib import sha256
private_key = pk.privkey.secret_multiplier 
digest = sha256(to_sign).digest()


# sign with recovery id
(r, s, rec_id) = secp256k1_generator.sign_with_recid(private_key, int.from_bytes(digest, 'big'))


# normalize to lower s
if s > secp256k1_generator.order()/2: #
    s = secp256k1_generator.order() - s
    rec_id ^= 1 # https://github.com/bitcoin-core/secp256k1/blob/e72103932d5421f3ae501f4eba5452b1b454cb6e/src/ecdsa_impl.h#L295
signature65 = r.to_bytes(32,byteorder='big')+s.to_bytes(32,byteorder='big')+rec_id.to_bytes(1,byteorder='big')


# post transaction request to warthog node
postdata = {
 "pinHeight": pinHeight,
 "nonceId": nonceId,
 "toAddr": toAddr,
 "amountE8": amountE8,
 "feeE8": feeE8,
 "signature65": signature65.hex()
}
rep = requests.post(baseurl + "/transaction/add", json = postdata)
rep.content
```
