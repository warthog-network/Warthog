args=['-DENABLE_MODULE_RECOVERY', '-DECMULT_GEN_PREC_BITS=4', '-DECMULT_WINDOW_SIZE=15', '-w']
# args=[ '-DECMULT_GEN_PREC_BITS=4', '-DECMULT_WINDOW_SIZE=15', '-w']
include_secp256k1=include_directories('./include')

libsecp256k1=library('warthogsecp256k1', 'src/secp256k1.cpp', include_directories:'./include', cpp_args: args,
  gnu_symbol_visibility:'hidden',
  install: true
  )

