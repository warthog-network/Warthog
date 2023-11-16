# Janushash - A Proof of Balanced Work Mining Algorithm
## What is Proof of Balanced Work?
Proof of Balanced Work (PoBW) is a novel method to base proof of work on a combination of multiple hash functions. Traditionally, only a single hash function is used for proof of work, such as double SHA256 for Bitcoin.

However it is possible to extend the concept of proof of work to multiple hash functions. This technique was first published in October 2023 by CoinFu Master Shifu in his work "[Proof of Balanced Work - The Theory of Mining Hash Products](https://github.com/CoinFuMasterShifu/ProofOfBalancedWork/blob/main/PoBW.pdf)". 

The method works by combining multiple hashes of different hash functions to *hash products* via multiplication, where the hashes are interpreted as numbers between 0 and 1. Mining such combinations
- will not suffer from bottlenecks,
- requires each component hash function to be efficiently computed and
- can therefore be used for example to create new mining algorithms that require both a capable CPU and GPU for efficient mining.

Miner developers are strongly encouraged to read this paper since it describes how classical PoW concepts transfer to the PoBW setting, analyzes best mining approaches and provides examples which contribute to a deeper understanding, in particulary why efficient mining of hash products requires mining resources to be appropriately balanced among the involved hash functions.


## What is Janushash?
Janushash is a particularly simple incarnation of Proof of Balanced Work because it only combines two different hash functions via a product. We have chosen two fast hash functions: Triple SHA256 and VerusHash v2.1. VerusHash v2.1 is mined particularly efficiently on Smarthpones, or generally ARM based devices and also on CPUs. On the other hand Triple SHA256 can be most efficiently computed on ASICS, however the multiplicative combination with VerusHash v2.1 will force miners to resort to GPUs since high bandwidth between the hardware mining VerusHash v2.1 is necessary to form hash products at high rate.

Therefore it is expected that the most efficient way to mine Janushash is a CPU/GPU combination. In this sense Janushash is the first proof of work algorithm which requires both, CPU and GPU capabilities at the same time. This fact coined the name "Janushash". 

The goal of using this PoBW mining algorithm is to avoid harmful centralization in mining, neigher GPU farms with poor CPUs nor CPU mining farms shall be able to efficiently mine this algorithm, instead the "little guys" shall be in the focus of mining.

From a high-level perspective computing a Janushash works by first computing both, the triple SHA256 hash and the Verushash v2.1 of an input byte sequence, and finally combining these two hashes into a new hash by multiplication:
<p align="center">
  <img src="img/janus.svg" style="width:500px;"/>
</p>

The multiplication is done as if the two computed hashes were floating-piont numbers between 0 and 1. The next section explains how exactly this is done.

## Specific implementation in Warthog

For PoBW verification a both hashes, Triple SHA256 and Verushash v2.1 are computed. Then both are converted into a custom floatin-point format called `HashExponentialDigest`:
```cpp
class HashExponentialDigest {
  friend struct Target;

public:
  uint32_t negExp{0}; // negative exponent of 2
  uint32_t data{0x80000000};

  HashExponentialDigest(){};
  HashExponentialDigest& digest(const Hash &);
};
```
It has two properties `negExp` and `data` which store the negative exponent and the mantissa of the stored values respectively. The default-constructed value corresponds to a stored value of 1. The conversion of a hash and the multiplication with the stored result is carried out in the `digest` function:
```cpp
inline HashExponentialDigest& HashExponentialDigest::digest(const Hash &h) {
  negExp += 1; // we are considering hashes as number in (0,1), padded with
               // infinite amount of trailing 1's
  size_t i = 0;
  for (; i < h.size(); ++i) {
    if (h[i] != 0)
      break;
    negExp += 8;
  }
  uint64_t tmpData{0};
  for (size_t j = 0;; ++j) {
    if (i < h.size())
      tmpData |= h[i++];
    else
      tmpData |= 0xFFu; // "infinite amount of trailing 1's"

    if (j >= 3)
      break;
    tmpData <<= 8;
  }
  size_t shifts = 0;
  while ((tmpData & 0x80000000ul) == 0) {
    shifts += 1;
    negExp += 1;
    tmpData <<= 1;
  }
  assert(shifts < 8);
  assert((tmpData >> 32) == 0);
  tmpData *= uint64_t(data);
  if (tmpData >= uint64_t(1) << 63) {
    tmpData >>= 1;
    negExp -= 1;
  }
  tmpData >>= 31;
  assert(tmpData < uint64_t(1) << 32);
  assert(tmpData >= uint64_t(1) << 31);
  data = tmpData;
  return *this;
};
```

A header then has valid Janushash PoBW if it passes this check (pseudocode):
```cpp
HashExponentialDigest hd; // stores 1 by default
hd.digest(verus_hash(header)); // now stores the verus hash
hd.digest(triple_sha256(header)); // now stores the hash product
return target.compatible(hd); // checks whether this satisfies the target
```
