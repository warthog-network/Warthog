#include "spdlog/spdlog.h"
class Address;

int start_gpu_miner(const Address&, std::string, uint16_t, std::string)
{
    spdlog::error("Miner was compiled without GPU support. GPU mining not available.");
    return -1;
}
