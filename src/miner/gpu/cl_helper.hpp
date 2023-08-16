#pragma once
#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#ifdef OPENCL_LEGACY
#include <CL/cl2.hpp>
#else
#include <CL/opencl.hpp>
#endif
#include <cassert>
namespace CL {
template <typename T> using vector = std::vector<T>;
class Device : public cl::Device {

public:
  using cl::Device::Device;
  auto name() const { return getInfo<CL_DEVICE_NAME>(); };
  auto vendor() const { return getInfo<CL_DEVICE_VENDOR>(); };
  auto version() const { return getInfo<CL_DEVICE_VERSION>(); };
  auto profile() const { return getInfo<CL_DEVICE_PROFILE>(); };
  auto driver_version() const { return getInfo<CL_DRIVER_VERSION>(); };
  auto extensions() const { return getInfo<CL_DEVICE_EXTENSIONS>(); };
  auto platform() const;
  auto str_info() const;
};
class Platform : public cl::Platform {

public:
  Platform(cl_platform_id id) : cl::Platform(id){};
  static vector<CL::Platform> all() {
    std::vector<CL::Platform> platforms;
    cl_uint n = 0;
    assert(CL_SUCCESS == clGetPlatformIDs(0, NULL, &n));
    std::vector<cl_platform_id> ids(n);
    assert(CL_SUCCESS == clGetPlatformIDs(n, ids.data(), NULL));
    platforms.reserve(ids.size());
    for (cl_platform_id id : ids)
      platforms.emplace_back(Platform{id});
    return platforms;
  };
  auto profile() const { return getInfo<CL_PLATFORM_PROFILE>(); };
  auto version() const { return getInfo<CL_PLATFORM_VERSION>(); };
  auto name() const { return getInfo<CL_PLATFORM_NAME>(); };
  auto vendor() const { return getInfo<CL_PLATFORM_VENDOR>(); };
  auto extensions() const { return getInfo<CL_PLATFORM_EXTENSIONS>(); };
  vector<CL::Device> devices(cl_device_type type = CL_DEVICE_TYPE_ALL) const {
    // cl::Platform::getDevices(type);
    vector<Device> devices;
    cl_uint n = 0;
    cl_int err = ::clGetDeviceIDs(object_, type, 0, NULL, &n);
    assert(err == CL_SUCCESS || err == CL_DEVICE_NOT_FOUND);

    vector<cl_device_id> ids(n);
    assert(n == 0 ||
           CL_SUCCESS == ::clGetDeviceIDs(object_, type, n, ids.data(), NULL));
    devices.reserve(ids.size());
    for (auto id : ids) {
      devices.push_back(Device(id, true));
    }
    return devices;
  }
};

inline auto Device::platform() const {
  return Platform{getInfo<CL_DEVICE_PLATFORM>()};
};

inline auto Device::str_info() const {
  return name() + " (using " + platform().version() + ")";
};

class Buffer : public cl::Buffer {
public:
  using cl::Buffer::Buffer;
  [[nodiscard]] size_t size() const { return getInfo<CL_MEM_SIZE>(); }
};
class CommandQueue : public cl::CommandQueue {
  void read_to(const Buffer &b, void* out, size_t len, bool blocking = true) {
    enqueueReadBuffer(b, blocking ? CL_TRUE : CL_FALSE, 0, len, out);
  }
public:
  using cl::CommandQueue::CommandQueue;
  auto context() const{
    return getInfo<CL_QUEUE_CONTEXT>();
  }
  template <typename T> [[nodiscard]] auto read(const Buffer &b) {
    const size_t rawSize{b.size()};
    assert(rawSize == sizeof(T));
    T out;
    read_to(b,&out,rawSize,true);
    return out;
  }
  template <typename T> [[nodiscard]] auto read_vector(const Buffer &b, bool blocking = true) {
    const size_t rawSize{b.size()};
    assert(rawSize % sizeof(T) == 0);
    const size_t numElements{rawSize / sizeof(T)};
    std::vector<T> out;
    out.resize(numElements);
    read_to(b,(void*)out.data(),rawSize,blocking);
    return out;
  }
};
} // namespace CL
