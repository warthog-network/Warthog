
include_wh=[include_trezorcrypto,include_directories('./src')]
src_wh=[
    src_trezorcrypto,
  files(
  [
    './platformassert.cpp',
  './src/block/body/container.cpp',
  './src/block/body/fee.cpp',
  './src/block/body/nonce.cpp',
  './src/block/body/transaction_id.cpp',
  './src/block/body/view.cpp',
  './src/block/chain/height.cpp',
  './src/block/chain/worksum.cpp',
  './src/block/header/generator.cpp',
  './src/block/header/header.cpp',
  './src/communication/create_payment.cpp',
  './src/crypto/address.cpp',
  './src/crypto/crypto.cpp',
  './src/crypto/hash.cpp',
  './src/general/errors.cpp',
  './src/general/funds.cpp',
  './src/general/hex.cpp',
  './src/general/with_uint64.cpp',
  ]
  )]
  vcs_dep=vcs_tag(command : 
    ['git', 'rev-parse', '--short', 'HEAD'],
    input:'./src/version.hpp.in',
    output:'version.hpp',
    replace_string:'@GIT_COMMIT_INFO@')
