# NodeJS Integration Guide
In this guide we demonstrate how to handle wallets and send transactions with NodeJS. We use the `elliptic`, `node:crypto`, `sync-request` and `secp256k1` NodeJS packages.

## Handling wallets

Below we demonstrate how to
- generate a new private key,
- load a private key from hexadecimal encoding, 
- derive a public key from a private key,
- derive a raw Warthog address form a public key and
- extend the raw Warthog address by its checksum to obtain an ordinary Warthog address. 
```javascript
const elliptic = require('elliptic');
const crypto = require('node:crypto');
const ec = new elliptic.ec('secp256k1');

//////////////////////////////
// Handling wallets
//////////////////////////////


// generate private key
var pk = ec.genKeyPair()

// alternatively read private key
var pkhex= '966a71a98bb5d13e9116c0dffa3f1a7877e45c6f563897b96cfd5c59bf0803e0'
var pk =  ec.keyFromPrivate(pkhex);

// convert private key to hex
pkhex = pk.getPrivate().toString("hex")
while (pkhex.length < 64) {
  pkhex = "0" + pkhex;
}

// print private key:
console.log("private key:", pkhex)

// derive public key
var pubKey = pk.getPublic().encodeCompressed("hex");

// print public key
console.log("public key:", pubKey)

// convert public key to raw addresss
var sha = crypto.createHash('sha256').update(Buffer.from(pubKey,"hex")).digest()
var addrRaw = crypto.createHash('ripemd160').update(sha).digest()

// generate address by appending checksum
var checksum = crypto.createHash('sha256').update(addrRaw).digest().slice(0,4)
var addr = Buffer.concat([addrRaw , checksum]).toString("hex")

// print address
console.log("address:", addr)

```

## Generating, signing and sending a transfer transaction

In Warthog transactions are pinned to a specific block the blockchain. The height of this block is its `pinHeight`, its hash the `pinHash`. This information has to be retrieved from the node in order to start generating a transaction, a transaction with a `pinHeight` that is too old be discarded. 
This serves two purposes
1. Users can be sure that transactions will not be pending forever.
2. By choosing a particular `pinHeight` users can actively control the time to live (TTL) of a pending transaction before it is discarded.

Not all height values are valid `pinHeights`, instead they must be a multiple of 32. For convenience the `/chain/head` endpoint provides the latest `pinHeight` value together with the corresponding block hash `pinHash`. In our code sample below we will use this endpoint.

In addition we demonstrate how to create the bytes to sign, how a valid sekp256k1 recoverable signature in custom format is generated and how to send the transaction to the Warthog node via a HTTP POST request. For reference read the [API guide](API.md). The below code uses the hexadecimal private key `pkhex` variable from the above code snippet.

**NOTE** Javascript does not support 64 bit integers natively, one has to resort to `BigInt`, which does not serialize as a JSON number required by the API. Therefore we are just serializing a normal Javascript number which will only be accurate up to 15 digits, i.e. only 7 figure WART amounts can be precisely handled this way because Warthog has a precision of 8 digits after comma. The total hard cap of WART is 18m which is a 8 figure number, so practically the 7 figures should be sufficient for most purposes.
```javascript
//////////////////////////////
// Generate a signed transaction
//////////////////////////////
var request = require('sync-request');
var baseurl = 'http://localhost:3000'

// get pinHash and pinHeight from warthog node
var head = JSON.parse(request('GET', baseurl + '/chain/head').body.toString())
var pinHeight = head.data.pinHeight
var pinHash = head.data.pinHash


// send parameters
var nonceId = 0 // 32 bit number, unique per pinHash and pinHeight
var toAddr = '0000000000000000000000000000000000000000de47c9b2' // burn destination address
var amountE8 = 100000000 // 1 WART, this must be an integer, coin amount * 10E8


// round fee from WART amount
var rawFee = "0.00009999" // this needs to be rounded, WARNING: NO SCIENTIFIC NOTATION
var result = request('GET',baseurl + '/tools/encode16bit/from_string/'+rawFee).body.toString()
var encode16bit_result = JSON.parse(result)
var feeE8 = encode16bit_result["data"]["roundedE8"] // 9992


// alternative: round fee from E8 amount
var rawFeeE8 = "9999" // this needs to be rounded
result = request('GET',baseurl + '/tools/encode16bit/from_e8/'+rawFeeE8).body.toString()
encode16bit_result = JSON.parse(result)
feeE8 = encode16bit_result["data"]["roundedE8"] // 9992


// generate bytes to sign
var buf1 = Buffer.from(pinHash,"hex")
var buf2 = Buffer.allocUnsafe(19)
buf2.writeUInt32BE(pinHeight,0)
buf2.writeUInt32BE(nonceId,4)
buf2.writeUInt8(0,8)
buf2.writeUInt8(0,9)
buf2.writeUInt8(0,10)
buf2.writeBigUInt64BE(BigInt(feeE8),11)
var buf3 = Buffer.from(toAddr.slice(0,40),"hex")
var buf4 = Buffer.allocUnsafe(8)
buf4.writeBigUInt64BE(BigInt(amountE8),0)
var toSign = Buffer.concat([buf1, buf2, buf3, buf4])


// sign with recovery id
const secp256k1 = require("secp256k1");
var signHash = crypto.createHash('sha256').update(toSign).digest()
var signed = secp256k1.ecdsaSign(signHash, Buffer.from(pkhex,"hex"));
var signatureWithoutRecid = signed.signature
var recid = signed.recid


// normalize to lower s
if (!secp256k1.signatureNormalize(signatureWithoutRecid))
    recid = recid ^ 1
var recidBuffer = Buffer.allocUnsafe(1)
recidBuffer.writeUint8(recid)


// form full signature
var signature65 = Buffer.concat([signatureWithoutRecid, recidBuffer])

// post transaction request to warthog node
var postdata = {
 "pinHeight": pinHeight,
 "nonceId": nonceId,
 "toAddr": toAddr,
 "amountE8": amountE8,
 "feeE8": feeE8,
 "signature65": signature65.toString("hex")
}

var res = request('POST', baseurl+'/transaction/add', { json: postdata }).body.toString()
console.log("send result: ", res)
```
