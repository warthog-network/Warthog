#include "api_call.hpp"
#include "block/body/view.hpp"
#include "block/header/generator.hpp"
#include "block/header/header_impl.hpp"
#include "block/header/view_inline.hpp"
#include "cmdline/cmdline.h"
#include "communication/create_payment.hpp"
#include "cpu/workerpool.hpp"
#include "crypto/crypto.hpp"
#include "general/hex.hpp"
#include "general/params.hpp"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <list>
#include <sstream>
using namespace std;

int start_gpu_miner(const Address& address, std::string host, uint16_t port, std::string gpus);
int process(gengetopt_args_info& ai)
{
    try {
        std::string host { ai.host_arg };
        uint16_t port(ai.port_arg);
        spdlog::info("Node RPC is {}:{}", host, port);
        if (ai.threads_arg < 0)
            throw std::runtime_error("Illegal value " + to_string(ai.threads_arg) + " for option --threads.");
        Address address(ai.address_arg);
        if (ai.gpu_given) { // GPU mining
            spdlog::info("GPU is used for mining.");
            if (ai.threads_given) {
                spdlog::warn("Ignoring --threads as this argument is for CPU mining.");
            }
            std::string gpus;
            if (ai.gpus_given) {
                gpus.assign(ai.gpus_arg);
            }
            start_gpu_miner(address,host,port,gpus);
        } else { // CPU mining
            spdlog::info("CPU is used for mining.");
            if (ai.gpus_given) {
                spdlog::warn("Ignoring --gpus as this argument is for GPU mining.");
            }
            size_t threads = ai.threads_arg;
            if (threads == 0)
                threads = std::thread::hardware_concurrency();
            spdlog::info("Starting worker pool with {} threads", threads);

            Workerpool wp(address, threads, host, port);
            wp.run();
        }
    } catch (std::runtime_error& e) {
        spdlog::error("{}", e.what());
        return -1;
    } catch (Error& e) {
        spdlog::error("{}", e.strerror());
        return -1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    srand(time(0));
    cout << "Miner ⚒ ⛏" << endl;
    gengetopt_args_info ai;
    if (cmdline_parser(argc, argv, &ai) != 0) {
        return -1;
    }
    int i = process(ai);
    cmdline_parser_free(&ai);
    return i;
}
