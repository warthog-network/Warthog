#pragma once

#include "secp256k1.h"
#include "secp256k1_preallocated.h"
#include "secp256k1_recovery.h"
#include "address.hpp"
#include <array>
#include <optional>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <sstream>

void ECC_Start();
void ECC_Stop();

class HashView;


// forward declaractions
class RecoverableSignature;

class PubKey {
  friend class PrivKey;
  friend class RecoverableSignature;

public:
  PubKey(const std::string &);
  bool operator==(const PubKey &rhs) const;
  Address address();
  std::string to_string() const;

private:
  PubKey(){};
  PubKey(const RecoverableSignature &recsig, HashView);
  std::array<uint8_t, 33> serialize() const;

private:
  secp256k1_pubkey pubkey;
};

class PrivKey {
public:
  PrivKey();
  PrivKey(const std::string);
  PrivKey(const uint8_t *pbegin, const uint8_t *pend);
  std::string to_string() const;
  friend bool operator==(const PrivKey &a, const PrivKey &b);
  PubKey pubkey() const;
  RecoverableSignature sign(HashView) const;
  std::array<uint8_t, 32> data() { return keydata; }

private: // private methods
  bool check(const uint8_t *vch);

private: // private data
  std::array<uint8_t, 32> keydata;
};

bool check_signature(const uint8_t *in65_signature,
                     HashView);

class RecoverableSignature {
public:
  static constexpr size_t length = 65;
  friend class PrivKey;
  friend class PubKey;
  RecoverableSignature(View<65>);
  RecoverableSignature(std::string_view);
  static std::optional<RecoverableSignature> from_view(View<65>);
  std::string to_string() const;
  void serialize(uint8_t *out65) const;
  std::array<uint8_t,65> serialize() const{
      std::array<uint8_t,65> res;
      serialize(res.data());
      return res;
  };
  PubKey recover_pubkey(HashView) const;

private:        // private methods
  RecoverableSignature(){}; //uninitialized
  bool construct(View<65>);
  bool check(); // check for lower S
  RecoverableSignature(const uint8_t *keydata, HashView);

private: // private data
  secp256k1_ecdsa_recoverable_signature recsig;
};
class Writer;
Writer& operator<<(Writer&, const RecoverableSignature&);
