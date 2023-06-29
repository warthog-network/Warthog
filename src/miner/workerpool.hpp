#pragma once

#include "general/hex.hpp"
#include "api_call.hpp"
#include "crypto/address.hpp"
#include "spdlog/fmt//fmt.h"
#include "spdlog/spdlog.h"
#include "worker.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <list>

inline std::pair<double, std::string_view> format_hashrate(size_t hr)
{
    using namespace std::literals;
    if (hr > 1000000000)
        return { double(hr) / 1000000000, "GH"sv };
    if (hr > 1000000)
        return { double(hr) / 1000000, "MH"sv };
    if (hr > 1000)
        return { double(hr) / 1000, "KH"sv };
    return { hr, "H"sv };
}
inline std::string format_duration(uint32_t seconds)
{
    if (seconds < 300) {
        return spdlog::fmt_lib::format("{} seconds", seconds);
    }
    if (seconds < 60 * 60)
        return spdlog::fmt_lib::format("{} minutes", uint32_t(10 * double(seconds) / 60.0) / 10.0);
    return spdlog::fmt_lib::format("{} hours", int32_t(10 * double(seconds) / 3600.0) / 10.0);
}

class Workerpool {
    friend struct PoolInterface;
    struct HashSnapshot {
        std::chrono::steady_clock::time_point t;
        uint64_t sumhashes;
        HashSnapshot(uint64_t sumhashes = 0)
            : t(std::chrono::steady_clock::now())
            , sumhashes(sumhashes)
        {
        }
        size_t update(uint64_t newHashes)
        {
            using namespace std::chrono;
            auto tmp_t(std::chrono::steady_clock::now());
            assert(tmp_t >= t);
            auto ms { duration_cast<milliseconds>(tmp_t - t).count() };
            if (ms == 0)
                ms += 1;
            size_t hashrate { 1000 * newHashes / ms };
            t = tmp_t;
            sumhashes += newHashes;
            return hashrate;
        }
    };

public:
    Workerpool(const std::string address, size_t threadnum, std::string host = "localhost", uint16_t port = 3000)
        : addrstr(address)
        , api(host, port)
    {
        for (size_t i = 0; i < threadnum; ++i) {
            workers.emplace_back(new Worker({ *this }));
        }
    };

private:
    HashSnapshot snapshot;
    std::mutex m;
    std::condition_variable cv;
    std::optional<Workertask> currentMiningTask;

    // mutex protected
    bool wakeup = false;
    bool _shutdown = false;
    std::list<Block> mined;

    size_t minedcount = 0;
    std::string addrstr;
    std::vector<std::unique_ptr<Worker>> workers;

    size_t update_hashrate()
    {
        std::vector<size_t> newhashes(workers.size());
        size_t totalNew = 0;
        for (size_t i = 0; i < workers.size(); ++i) {
            size_t tmp = workers[i]->update_hashes();
            newhashes[i] = tmp;
            totalNew += tmp;
        }
        return snapshot.update(totalNew);
    }
    void on_mined(Block mt)
    {
        std::unique_lock l(m);
        mined.push_back(std::move(mt));
        wakeup = true;
        cv.notify_one();
    }

public:
    void run()
    {
        using namespace std::chrono;
        bool updateMining = true;
        time_point<steady_clock> until;
        while (true) {
            bool expired { steady_clock::now() > until };
            if (expired) {
                until = steady_clock::now() + seconds(5);
                auto rawhr { update_hashrate() };
                auto [hr, unit] = format_hashrate(rawhr);
                std::string durationstr;
                if (currentMiningTask) {
                    uint32_t seconds(currentMiningTask->b.header.target().difficulty() / rawhr);
                    durationstr = spdlog::fmt_lib::format("(~{} per block)", format_duration(seconds));
                }
                spdlog::info("Hashrate: {} {}/s {}", hr, unit, durationstr);
            }
            if (updateMining || expired) {
                updateMining = false;
                currentMiningTask = Workertask { api.get_mining(Address(addrstr)) };
                for (auto& w : workers) {
                    w->set_work(*currentMiningTask);
                }
            }
            std::unique_lock l(m);
            while (!wakeup && steady_clock::now() < until) {
                cv.wait_until(l, until);
            }
            wakeup = false;
            if (_shutdown) {
                workers.clear();
                break;
            }
            while (!mined.empty()) {
                Block& b = mined.front();

                if (currentMiningTask->b.height == b.height) {
                    for (auto& w : workers)
                        w->disable();
                    updateMining = true;
                    spdlog::debug("#{} Submitting mined block at height {}...", ++minedcount, b.height.value());
                    spdlog::info("block hash {}, {}", serialize_hex(b.header.hash()), b.header.nonce());
                    auto res = api.submit_block(b);
                    if (res.second == 0) {
                        spdlog::info("💰 #{} Mined block {}.", minedcount, b.height.value());
                        // exit(0);
                    } else {
                        spdlog::warn("⚠  #{} Mined block {} rejected: {}", minedcount, b.height.value(), res.first);
                    }
                } else {
                    // outdated mining solution
                }
                mined.pop_front();
            }
        }
    }
    API api;
};
