# Elixir Integration Guide
In this guide we demonstrate how to handle wallets and send transactions with Elixir. We use the `{:curvy, "~> 0.3.1"}`, `{:jason, "~> 1.4"}` and `{:httpoison, "~> 2.0"}` Hex packages.

## Handling wallets

Below we demonstrate how to
- generate a new private key,
- load a private key from hexadecimal encoding, 
- derive a public key from a private key,
- derive a raw Warthog address form a public key and
- extend the raw Warthog address by its checksum to obtain an ordinary Warthog address. 
```elixir
##############################
# Handling wallets
##############################
#
# generate private key
pk = Curvy.generate_key()

# alternatively read private key
pk = "966a71a98bb5d13e9116c0dffa3f1a7877e45c6f563897b96cfd5c59bf0803e0" |> Base.decode16!(case: :lower) |> Curvy.Key.from_privkey()

# print private key
Curvy.Key.to_privkey(pk)|>Base.encode16|>IO.puts

# derive public key
pubkey = Curvy.Key.to_pubkey(pk)

# print public key
pubkey|>Base.encode16|>IO.puts

# convert public key to raw addresss
addr_raw = :crypto.hash(:ripemd160,:crypto.hash(:sha256,pubkey))

# generate address by appending checksum
<< checksum :: binary-size(4),_::binary>> =  :crypto.hash(:sha256,addr_raw)
addr= addr_raw<>checksum

# print address
IO.puts("address: #{addr|>Base.encode16()}")
```

## Generating, signing and sending a transfer transaction

In Warthog transactions are pinned to a specific block the blockchain. The height of this block is its `pinHeight`, its hash the `pinHash`. This information has to be retrieved from the node in order to start generating a transaction, a transaction with a `pinHeight` that is too old be discarded. 
This serves two purposes
1. Users can be sure that transactions will not be pending forever.
2. By choosing a particular `pinHeight` users can actively control the time to live (TTL) of a pending transaction before it is discarded.

Not all height values are valid `pinHeights`, instead they must be a multiple of 32. For convenience the `/chain/head` endpoint provides the latest `pinHeight` value together with the corresponding block hash `pinHash`. In our code sample below we will use this endpoint.

In addition we demonstrate how to create the bytes to sign, how a valid sekp256k1 recoverable signature in custom format is generated and how to send the transaction to the Warthog node via a HTTP POST request. For reference read the [API guide](API.md). The below code uses the private key `pk` variable from the above code snippet.

```elixir
##############################
# Generate a signed transaction
##############################
baseurl = "http://localhost:3000"


# get pinHash and pinHeight from warthog node
head = HTTPoison.get!(baseurl <> "/chain/head").body|>Jason.decode!()
pinHash =  head["data"]["pinHash"]
pinHeight = head["data"]["pinHeight"]


# send parameters
nonceId = 0 # 32 bit number, unique per pinHash and pinHeight
toAddr = "0000000000000000000000000000000000000000de47c9b2" # burn destination address
amountE8 = 100000000 # 1 WART, this must be an integer, coin amount * 10E8


# round fee from WART amount
rawFee = "0.00009999" # this needs to be rounded, WARNING: NO SCIENTIFIC NOTATION
result = HTTPoison.get!(baseurl<>"/tools/encode16bit/from_string/#{rawFee}").body
encode16bit_result = Jason.decode!(result)
feeE8 = encode16bit_result["data"]["roundedE8"] # 9992


# alternative: round fee from E8 amount
rawFeeE8 = "9999" # this needs to be rounded
result = HTTPoison.get!(baseurl<>"/tools/encode16bit/from_e8/#{rawFeeE8}").body
encode16bit_result = Jason.decode!(result)
feeE8 = encode16bit_result["data"]["roundedE8"] # 9992


# generate bytes to sign
to_sign = 
  Base.decode16!(pinHash, case: :lower) <> 
  <<pinHeight ::big-unsigned-size(32)>> <>
  <<nonceId ::big-unsigned-size(32)>> <>
  <<0 ::big-unsigned-size(24)>> <>
  <<feeE8 ::big-unsigned-size(64)>> <>
    binary_part(toAddr|>Base.decode16!(case: :lower),0,20) <>
  <<amountE8 ::big-unsigned-size(64)>>

# create normalized signature
<<recid::size(8), rs::binary-size(64)>> = Curvy.sign(to_sign, pk, hash: :sha256, compact: true, normalize: true)
signature65 = rs <> <<(recid-31) ::8>> 


# post transaction request to warthog node
postdata = %{
 pinHeight: pinHeight,
 nonceId: nonceId,
 toAddr: toAddr,
 amountE8: amountE8,
 feeE8: feeE8,
 signature65: signature65 |> Base.encode16
}
HTTPoison.post!(baseurl <> "/transaction/add", Jason.encode!(postdata)).body
```
