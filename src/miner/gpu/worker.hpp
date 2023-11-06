#pragma once
#include "block/block.hpp"
#include "cl_function.hxx"
#include "cl_helper.hpp"
#include "general/hex.hpp"
#include "kernel.hpp"
#include "spdlog/spdlog.h"
#include <arpa/inet.h>
#include <condition_variable>
#include <iostream>
#include <optional>
#include <span>
#include <thread>
class MinerDevice {
    static constexpr size_t numSlots = 8;
    static cl::Program::Sources fetch_sources()
    {
        std::string code { kernel, sizeof(kernel) };
        // auto code{read_file("kernel.cl")};
        return cl::Program::Sources { { code.data(), code.size() } };
    };
    auto build_program(cl::Context context)
    {
        cl::Program program(context, fetch_sources());
        try {
            program.build(
                "-cl-std=CL2.0 -DVECT_SIZE=2 -DDGST_R0=3 -DDGST_R1=7 -DDGST_R2=2 "
                "-DDGST_R3=6 -DDGST_ELEM=8 -DKERNEL_STATIC");
            return program;
        } catch (cl::BuildError& e) {
            auto logs { e.getBuildLog() };
            assert(logs.size() == 1);
            auto& log = logs[0].second;
            std::cerr << " Build error: " << log << std::endl
                      << std::flush;

            throw e;
        }
    }

public:
    MinerDevice(cl::Device device)
        : context({ device })
        , program(build_program(context))
        , queue(context, device)
        , reset_counter_fun(program, "reset_counter")
        , set_target_fun(program, "set_target")
        , mine_fun(program, "mine") {};
    void set_block_header(std::span<uint8_t, 76> h)
    {
        memcpy(blockHeader.data(), h.data(), h.size());
    }
    void set_target(uint32_t v)
    {
        cl::EnqueueArgs nd1(queue, cl::NDRange(1));
        set_target_fun.run(queue, nd1, v);
    }

    auto mine(uint32_t nHashes, uint32_t offset)
    {
        cl::EnqueueArgs eargs(queue,
            offset == 0 ? cl::NullRange : cl::NDRange(offset),
            cl::NDRange(nHashes), cl::NullRange);
        return mine_fun.run(queue, eargs, blockHeader);
    }
    auto reset_counter()
    {
        cl::EnqueueArgs nd1(queue, cl::NDRange(1));
        return reset_counter_fun.run(queue, nd1);
    }

private:
    cl::Context context;
    cl::Program program;

    std::array<uint8_t, 76> blockHeader;
    CL::CommandQueue queue;
    CLFunction<>::Returning<uint32_t> reset_counter_fun;
    CLFunction<uint32_t>::Returning<> set_target_fun;
    // CLFunction<std::array<uint8_t, 76>>::Returning<> set_block_header_fun;
    CLFunction<std::array<uint8_t, 76>>::Returning<std::array<uint32_t, numSlots>,
        std::array<std::array<uint8_t, 32>, numSlots>>
        mine_fun;
};
class DevicePool;
class DeviceWorker {
public:
    DeviceWorker(const CL::Device& device, DevicePool& pool)
        : pool(pool)
        , miner(device)
        , deviceName(device.name())
    {
        tune();
    };
    DeviceWorker(const DeviceWorker&) = delete;
    DeviceWorker(DeviceWorker&&) = delete;
    ~DeviceWorker()
    {
        thread.request_stop();
        wakeup_nolock();
    }

    uint64_t get_hashrate()
    {
        if (!lastHashrateCheckpoint.has_value())
            return 0;
        using namespace std::chrono;
        auto hashes { hashCounter.exchange(0) };
        auto now { steady_clock::now() };
        if (hashes == 0)
            return 0;
        auto ms = duration_cast<milliseconds>(now - *lastHashrateCheckpoint).count();
        lastHashrateCheckpoint = now;
        if (ms == 0)
            return 0;
        return (hashes * 1000) / ms;
    }

    void set_block(const Block& b)
    {
        std::lock_guard l(m);
        nextTask = b;
        wakeup_nolock();
    };

    void start_mining(){
        thread = std::jthread([=, this]() { run(); });
    }
private:
    void tune() {
        using namespace std::literals::chrono_literals;
        using namespace std::chrono;
        spdlog::info("Tuning {}.", deviceName);
        while (true) {
            auto start = std::chrono::steady_clock::now();
            miner.mine(hashesPerStep, 0);
            auto duration { steady_clock::now() - start };
            if (duration > 100ms || 2 * size_t(hashesPerStep) > std::numeric_limits<uint32_t>::max())
                break;
            hashesPerStep *= 2;
        }
    }

    void run()
    {
        using namespace std::literals::chrono_literals;
        using namespace std::chrono;
        auto stop_token { thread.get_stop_token() };
        while (true) {
            decltype(nextTask) tmpTask;
            {
                std::unique_lock l(m);
                tmpTask = nextTask;
                nextTask.reset();
            }
            if (stop_token.stop_requested()) {
                return;
            }
            if (tmpTask.has_value()) {
                currentTask = *tmpTask;
                init_mining(miner);
            }
            if (currentTask) {
                if (!lastHashrateCheckpoint.has_value()) {
                    lastHashrateCheckpoint = std::chrono::steady_clock::now();
                    spdlog::info("Now mining on {}.", deviceName);
                }
                mine(miner);
            } else {
                std::this_thread::sleep_for(100ms);
            }
        }
    }
    void wakeup_nolock()
    {
        wakeup = true;
        cv.notify_one();
    }
    void mine(MinerDevice& miner)
    {
        if (hashesTried == std::numeric_limits<uint32_t>::max()) {
            init_mining(miner);
        }
        auto nHashes { std::min(std::numeric_limits<uint32_t>::max() - hashesTried,
            hashesPerStep) };
        auto [args, hashes] = miner.mine(nHashes, hashesTried + randOffset);
        auto [found] = miner.reset_counter();
        if (found > 0) {
            auto& h { currentTask->header };
            h.set_nonce(hton32(args[0]));
            notify_mined(currentTask.value());
        }

        hashesTried += nHashes;
        hashCounter += nHashes;
    };

    void init_mining(MinerDevice&);
    void notify_mined(const Block&);

    // thread variables
    uint32_t hashesPerStep { 1u };
    uint32_t hashesTried { 0 };
    uint32_t randOffset { 0 };
    std::optional<Block> nextTask;
    DevicePool& pool;
    MinerDevice miner;

    // external variables
    std::optional<std::chrono::steady_clock::time_point> lastHashrateCheckpoint;

    // atomic shared variables
    std::atomic<uint64_t> hashCounter { 0 };

    // shared variables
    std::mutex m;
    bool wakeup = false;
    std::condition_variable cv;
    std::optional<Block> currentTask;
    std::jthread thread;

    // const variables
public:
    const std::string deviceName;
};
