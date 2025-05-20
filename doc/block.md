# Block Structure

## Block structure V1 ##
+ `4` bytes for mining
+ `4 + 20*i` bytes **Address Section**:
    + `4` bytes encode `i` (number of addresses)
    + `20*i` bytes for `i` addresses
+ `2 + 16` bytes **Reward Section**:
    + `2` bytes reserved
    + `8` bytes for *account id*
    + `8` bytes for *WART amount*
+ `4 + 99*i` bytes **WART Transfer Section**:
    + `4` bytes encode `i`
    + `99*i` bytes for `i` *transfer entries*:
        + `8` bytes for the origin account id
        + `8` bytes for *PinNonce*
        + `2` bytes for transaction fee
        + `8` bytes for destination account id
        + `8` bytes for WART amount
        + `65` bytes for the signature

## Block structure V2 ##
+ **`10`** bytes for mining
+ **`2`**` + 20*i` bytes **Address Section**:
    + `2` bytes encode `i` (number of addresses)
    + `20*i` bytes for `i` addresses
+ `16` bytes **Reward Section**:
    + `8` bytes for *account id*
    + `8` bytes for *WART amount*
+ `0` or `4 + 99*i` bytes **WART Transfer Section**:
    + `4` bytes encode `i`
    + `99*i` bytes for `i` *transfer entries*:
        + `8` bytes for the origin account id
        + `8` bytes for *PinNonce*
        + `2` bytes for transaction fee
        + `8` bytes for destination account id
        + `8` bytes for WART amount
        + `65` bytes for the signature
#### Differences between V2 and V1:
 - 10 bytes for mining instead of 4
 - 2 bytes for address section (max 65535 addresses) instead of 4 becaause block size limits this anyways)
 - Reward Section no longer has 2 reserved bytes
 - Transfer Section can collapse to 0 bytes when there are no transfers

 ## Block structure V4 ##
+ `10` bytes for mining
+ `2 + 20*i` bytes **Address Section**:
    + `2` bytes encode `i` (number of addresses)
    + `20*i` bytes for `i` addresses
+ `16` bytes **Reward Section**:
    + `8` bytes for *account id*
    + `8` bytes for *WART amount*
+ `0` or `4 + 99*i` bytes **WART Transfer Section**:
    + `4` bytes encode `i`
    + `99*i` bytes for `i` *transfer entries*:
        + `8` bytes for the origin account id
        + `8` bytes for *PinNonce*
        + `2` bytes for transaction fee
        + `8` bytes for destination account id
        + `8` bytes for WART amount
        + `65` bytes for the signature
+ `2 + <i per token sections>` **Token Section**
    + `2` bytes encode `i`.
    + `4 + 5 +j + k + l +m` for each of `<i per token sections>`
        + `4` bytes encode token id
        + `5` bytes encode `j`, `k`, `l`, `m` (10 bits each)
        + `99*j` bytes for `j` *Token Transfer Entries*:
            + `8` bytes for the origin account id
            + `8` bytes for *PinNonce*
            + `2` bytes for transaction fee
            + `8` bytes for destination account id
            + `8` bytes for WART amount
            + `65` bytes for the signature
TODO
        + `?*k` bytes encode `k` *Token/WART Order Entries*
        + `?*l` bytes encode `l` *Token/WART Liquidity Add Entries*
        + `?*m` bytes encode `m` *Token/WART Liquidity Remove Entries*
