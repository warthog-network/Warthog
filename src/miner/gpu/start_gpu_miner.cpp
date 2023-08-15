#include "gpu/cl_function.hxx"
#include "gpu/pool.hpp"
#include "gpu/worker.hpp"
#include "helpers.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <vector>

using namespace std;
auto all_gpu_devices()
{
    std::vector<CL::Device> v;
    for (auto& p : CL::Platform::all()) {
        for (auto& d : p.devices(CL_DEVICE_TYPE_GPU)) {
            v.push_back(std::move(d));
        }
    }
    return v;
}

auto read_file(std::string path)
{
    std::vector<char> v;
    if (FILE* fp = fopen(path.c_str(), "r")) {
        char buf[1024];
        while (size_t len = fread(buf, 1, sizeof(buf), fp))
            v.insert(v.end(), buf, buf + len);
        fclose(fp);
    }
    return v;
}

int start_gpu_miner(const Address& address, std::string host, uint16_t port)
{
    srand(time(0));

    using namespace std::chrono;
    auto gpu_devices { all_gpu_devices() };
    if (gpu_devices.empty()) {
        std::cerr << "No GPUs detected. Check OpenCL installation!\n";
        return -1;
    }
    auto& gpu = gpu_devices.front();
    cout << "Using the following GPU:\n";
    cout << gpu.name() << endl;
    std::vector<CL::Device> dv { gpu_devices.back() };
    DevicePool(address, gpu_devices.front(), host, port).run();
    return 0;
}
