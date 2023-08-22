#include "gpu/cl_function.hxx"
#include "gpu/pool.hpp"
#include "gpu/worker.hpp"
#include "helpers.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
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

[[nodiscard]] std::set<uint32_t> parse_gpus(std::string gpus)
{
    std::set<uint32_t> s;
    std::size_t p0 = 0;
    while (true) {
        auto p1 = gpus.find(",", p0);
        std::string istr { gpus.substr(p0, p1 - p0) };
        p0 = p1 + 1;
        if (istr.size() != 0) {
            uint32_t v;
            auto res = std::from_chars(istr.data(), istr.data() + istr.size(), v);
            if (res.ptr != istr.data() + istr.size())
                throw std::runtime_error("Cannot parse GPU list.");
            if (s.insert(v).second == false)
                spdlog::warn("GPU id {} was specified multiple times", v);
        }
        if (p1 == std::string::npos)
            break;
    }
    return s;
}

int start_gpu_miner(const Address& address, std::string host, uint16_t port, std::string gpus)
{
    srand(time(0));

    using namespace std::chrono;
    auto gpu_devices { all_gpu_devices() };
    if (gpu_devices.empty()) {
        std::cerr << "No GPUs detected. Check OpenCL installation!\n";
        return -1;
    }
    cout << "OpenCL installations for the following GPUs were detected:\n";
    for (size_t i = 0; i < gpu_devices.size(); ++i) {
        auto& d { gpu_devices[i] };
        cout << "[" << i << "]: " << d.name() << endl;
    }
    std::vector<CL::Device> dv;
    ;
    if (gpus.size() == 0) {
        cout << "Using all GPUs." << endl;
        dv = gpu_devices;
    } else {
        cout << "Using specified GPUs:" << endl;
        auto gpuset { parse_gpus(gpus) };
        for (size_t i = 0; i < gpu_devices.size(); ++i) {
            if (!gpuset.contains(i))
                continue;
            auto& d { gpu_devices[i] };
            dv.push_back(d);
            cout << "[" << i << "]: " << d.name() << endl;
        }
        if (dv.size() == 0) {
            spdlog::error("No GPUs selected.");
            return -1;
        }
    }

    DevicePool(address, dv, host, port).run();
    return 0;
}
