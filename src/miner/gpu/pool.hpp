#pragma once
#include "api_call.hpp"
#include "block/block.hpp"
#include "block/header/difficulty.hpp"
#include "block/header/header_impl.hpp"
#include "crypto/address.hpp"
#include "helpers.hpp"
#include "worker.hpp"
#include <iostream>
#include <vector>
class DevicePool {
    friend class DeviceWorker;

public:
    DevicePool(const Address& address, CL::Device& device, std::string host, uint16_t port)
        : worker(device,*this),
         address(address)
        , api(host, port)
    {}

    void notify_mined(const Block& b)
    {
        std::lock_guard l(m);
        minedBlock = b;
        wakeup_nolock();
    }
    void notify_shutdown()
    {
        shutdown = true;
        cv.notify_one();
    }

    void assign_work(const Block& b)
    {
        if (task.has_value() && b.header == task->header)
            return;
        blockSeed = randuint32() % 2000;
        task = b;
        worker.set_block(b);
    }

    void run()
    {
        using namespace std::literals::chrono_literals;
        using namespace std::chrono;

        constexpr auto printInterval { 10s };
        constexpr auto pollInterval { 500ms };
        auto nextPrint = steady_clock::now() + printInterval;
        auto nextPoll = steady_clock::now() + pollInterval;
        while (true) {
            decltype(minedBlock) tmpMined;
            {
                std::unique_lock ul(m);
                while (true) {
                    if (shutdown)
                        return;

                    if (steady_clock::now() >= nextPrint) {
                        print_hashrate();
                        nextPrint = steady_clock::now() + printInterval;
                    }
                    if (steady_clock::now() >= nextPoll) {
                        wakeup = true;
                    }
                    if (wakeup) {
                        wakeup = false;
                        break;
                    }
                    cv.wait_until(ul, std::min(nextPrint, nextPoll));
                }
                tmpMined = minedBlock;
                minedBlock.reset();
            }
            // did waked up

            bool submitted = false;
            if (tmpMined && task && (tmpMined->height == task.value().height)) {
                auto& b { *tmpMined };
                spdlog::debug("#{} Submitting mined block at height {}...", ++minedcount, b.height.value());
                auto res { api.submit_block(*tmpMined) };
                if (res.second == 0) {
                    spdlog::info("💰 #{} Mined block {}.", minedcount, b.height.value());
                    // exit(0);
                } else {
                    spdlog::warn("⚠  #{} Mined block {} rejected: {}", minedcount, b.height.value(), res.first);
                }
                submitted = true;
            }
            if (submitted || steady_clock::now() >= nextPoll) {
                nextPoll = steady_clock::now() + pollInterval;
                auto block = api.get_mining(address);
                assign_work(block);
            }
        }
        std::cout << "End" << std::endl;
    }

private:
    void print_hashrate()
    {
        auto hashrate = worker.get_hashrate();

        std::string durationstr;
        if (task.has_value()) {
            uint32_t seconds(task->header.target().difficulty() / hashrate);
            durationstr = spdlog::fmt_lib::format("(~{} per block)", format_duration(seconds));
        }
        auto [val, unit] = format_hashrate(hashrate);
        spdlog::info("{} hashrate: {} {}/s {}", worker.deviceName, val, unit, durationstr);
    }
    void wakeup_nolock()
    {
        wakeup = true;
        cv.notify_one();
    }
    std::condition_variable cv;
    std::mutex m; // recursive because lock from signal handler can occurr
    bool wakeup { false };
    std::atomic<bool> shutdown { false };
    std::optional<Block> minedBlock;

    std::atomic_int64_t blockSeed { 0 };
    uint64_t minedcount { 0 };
    std::optional<Block> task;
    DeviceWorker worker;
    Address address;
    API api;
};
