#include "general/hex.hpp"
#include "verus_clhash_opt.hpp"
#include "crypto/verushash/verushash.hpp"
#include <chrono>
#include <iostream>
#include <string>

int main()
{
    using namespace Verus;
    using namespace std;
    VerusHasher v;
    std::string s("Test1234Test1234Test1234Test1234Test1234Test1234Test1234Test12"
                  "34Test1234Test1234Test1234Test1234");
    auto hash { verus_hash(std::span<uint8_t>((uint8_t*)s.data(), s.size())) };

    std::array<uint8_t, 80> a { 0 };
    MinerOpt miner(a, hash, 0);

    auto start { std::chrono::steady_clock::now() };
    if (auto res { miner.mine(10000000) }; res.has_value()) {
        uint32_t count = miner.last_count();
        auto v { res.value() };
        auto stop { std::chrono::steady_clock::now() };
        size_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start)
                        .count();
        cout << "count : " << count << endl;
        cout << "Time: " << ms << " ms" << endl;
        cout << "Hashrate: " << (1000 * count) / (ms + 1) << "h/s" << endl;
        cout << "out: " << serialize_hex(v.hash) << endl;
        cout << "h2: " << serialize_hex(verus_hash(v.arg)) << endl;
    };

    cout << "Can optimize: " << can_optimize() << endl;
    cout << serialize_hex(hash) << endl;
    return 0;
}
