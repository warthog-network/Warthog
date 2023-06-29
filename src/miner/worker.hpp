#pragma once
#include "block/block.hpp"
#include <condition_variable>
#include "block/header/header_impl.hpp"
#include <memory>
#include <mutex>
#include <thread>

struct Workertask {
    Block b;
    std::shared_ptr<std::atomic<uint32_t>> seeds;
    Workertask(Block b)
        : b(b)
        , seeds(std::make_shared<std::atomic<uint32_t>>(0))
    {
    }
};
class Workerpool;
struct PoolInterface {
    void on_mined(Block mt);
    uint32_t next_seed();
    Workerpool& pool;
};

class Worker {

public:
    Worker(PoolInterface pool)
        : pool(pool)
    {
        std::thread t2(&Worker::work, this);
        t.swap(t2);
    };
    ~Worker()
    {
        terminate();
        t.join();
    }

    void disable()
    {
        std::unique_lock l(m);
        sleep = true;
        wakeup = true;
        cv.notify_one();
    }
    void set_work(Workertask t)
    {
        std::unique_lock l(m);
        wakeup = true;
        sleep = false;
        newTask = true;
        workerTask = std::move(t);
        cv.notify_one();
    }
    [[nodiscard]] size_t update_hashes()
    {
        size_t tmp { hash_snapshot };
        size_t out = sumhashes - tmp;
        sumhashes = tmp;
        return out;
    }

private:
    std::thread t;

    // not accessed by worker thread
    uint64_t hash_snapshot { 0 };

    // worker thread owned

    // mutex protected
    std::mutex m;
    std::optional<Workertask> workerTask;
    bool newTask;
    bool wakeup = false;
    bool shutdown = false;
    bool sleep = true;

    // shared atomic
    std::atomic<uint64_t> sumhashes { 0 };

    std::condition_variable cv;
    PoolInterface pool;
    void terminate()
    {
        std::unique_lock l(m);
        shutdown = true;
        wakeup = true;
        cv.notify_one();
    }
    void work();
};
