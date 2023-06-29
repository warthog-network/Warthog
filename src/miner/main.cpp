#include "api_call.hpp"
#include "block/body/view.hpp"
#include "block/header/generator.hpp"
#include "block/header/header_impl.hpp"
#include "block/header/view_inline.hpp"
#include "cmdline/cmdline.h"
#include "communication/create_payment.hpp"
#include "crypto/crypto.hpp"
#include "general/hex.hpp"
#include "general/params.hpp"
#include "mine.hpp"
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
#include "workerpool.hpp"
using namespace std;

int process(gengetopt_args_info& ai)
{
    try {
        if (ai.threads_arg < 0)
            throw std::runtime_error("Illegal value " + to_string(ai.threads_arg) + " for option --threads.");
        size_t threads = ai.threads_arg;
        std::string host{ai.host_arg};
        uint16_t port(ai.port_arg);
        if (threads == 0)
            threads = std::thread::hardware_concurrency();
        spdlog::info("Node RPC is {}:{}",host,port);
        spdlog::info("Starting worker pool with {} threads", threads);

        Workerpool wp(ai.address_arg, threads,host,port);
        wp.run();
    } catch (std::runtime_error& e) {
        spdlog::error("{}",e.what());
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
