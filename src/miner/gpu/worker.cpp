#include "worker.hpp"
// #include <arpa/inet.h>
#include "block/header/difficulty.hpp"
#include "block/header/header_impl.hpp"
#include "helpers.hpp"
#include "pool.hpp"
#include "block/body/view.hpp"
#include <iostream>

void DeviceWorker::init_mining(MinerDevice& miner)
{
    hashesTried = 0;
    uint32_t newSeed( (pool.blockSeed++) % 2000);

    auto &b = currentTask.value();
    memcpy(b.body.data().data(), &newSeed, 4);
    BodyView bv(b.body.view());

    randOffset = randuint32();
    b.header.set_merkleroot(bv.merkleRoot());

    std::span<uint8_t,76> c(b.header.data(),b.header.data()+76);
    miner.set_block_header(c);
    miner.set_target(b.header.target().binary());
}

void DeviceWorker::notify_mined(const Block& b)
{
    pool.notify_mined(b);
};
