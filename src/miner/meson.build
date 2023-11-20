

cpp_args = []
if get_option('enable-gpu-miner') == true
  message('GPU mining is enabled.')
  kernel_files = files(
  './gpu/third_party/hashcat/inc_vendor.h',
  './gpu/third_party/hashcat/inc_types.h',
  './gpu/third_party/hashcat/inc_platform.h',
  './gpu/third_party/hashcat/inc_common.h',
  './gpu/third_party/hashcat/inc_hash_sha256.h',
  './gpu/third_party/hashcat/inc_platform.cl',
  './gpu/third_party/hashcat/inc_common.cl',
  './gpu/third_party/hashcat/inc_hash_sha256.cl',
  './gpu/kernel.cl',
  )
  if get_option('opencl-legacy') == true
    cpp_args = [cpp_args,'-DOPENCL_LEGACY']
  endif
  kernelcat = run_command( ['sh', files('./gpu/generate_kernel.sh'), kernel_files], check: true).stdout().strip()
  conf_data = configuration_data()
  conf_data.set('KERNEL_SRC', kernelcat)
  configure_file(input : 'gpu/kernel.hpp.in',
                 output : 'kernel.hpp',
                 configuration : conf_data)
  gpusrc = [
    './gpu/start_gpu_miner.cpp',
    './gpu/worker.cpp'
    ]
  opencl_dep = dependency('OpenCL')
  dep = [ 
    libuv_dep,
    opencl_dep]
else
  message('GPU mining is disabled.')
  gpusrc = [
    './gpu/disabled.cpp',
    ]
  dep = [ 
    libuv_dep]
endif
cpusrc = [
    './cpu/third_party/cpuminer-opt/sph_sha2.c',
    './cpu/third_party/cpuminer-opt/sha256-hash-opt.c',
    './cpu/worker.cpp', 
    './cpu/mine.cpp', 
    ]

executable('wart-miner', vcs_dep, 
  [
    './api_call.cpp', 
    './cmdline/cmdline.cpp', 
    './main.cpp', 
    gpusrc,
    cpusrc,
    src_wh, 
    src_spdlog
    ],
  c_args: ['-march=native'],
  cpp_args: cpp_args,
  include_directories : [include_spdlog, include_secp256k1, include_wh,include_json, include_httplib],
  link_with: libsecp256k1, 
  dependencies: dep,
  install : true)
