#include "worker.hpp"
#include "block/body/view.hpp"
#include "helpers.hpp"
#include "mine.hpp"
#include "spdlog/spdlog.h"
#include "workerpool.hpp"

void PoolInterface::on_mined(Block mt)
{
    pool.on_mined(mt);
};

void Worker::work()
{
    std::optional<uint32_t> startnonce;
    std::optional<Workertask> task;
    while (true) {
        {
            std::unique_lock l(m);
            while (!wakeup && sleep) {
                cv.wait(l);
            }
            wakeup = false;
            if (newTask) {
                newTask = false;
                task = workerTask;
                startnonce.reset();
            }
            if (shutdown)
                break;
            if (sleep)
                continue;
        }

        if (!startnonce.has_value()) {
            startnonce = randuint32();
            uint32_t i = (*task->seeds)++;
            memcpy(task->b.body.data().data(), &i, 4);
            BodyView bv(task->b.body.view());
            task->b.header.set_merkleroot(bv.merkleRoot());
            task->b.header.set_nonce(*startnonce);
        }

        auto [solved, exhausted, tries] = mine(task->b.header, *startnonce, 10000);
        if (solved) {
            {
                std::unique_lock l(m);
                sleep = true; // solved block wait for next work
            }
            pool.on_mined(task->b);
        }
        if (exhausted) {
            startnonce.reset();
        }
        sumhashes += tries;
    }
}
