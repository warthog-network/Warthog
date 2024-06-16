#include "browser.hpp"
#include <iostream>
using namespace std;
std::vector<std::string> ws_peers()
{
    std::vector<std::string> res;
    auto s { getenv("WS_PEERS") };
    cout << "WS_PEERS: " << s << endl;
    const size_t N = strlen(s);
    if (!s)
        return {};
    size_t i0 = 0;
    size_t i = 1;
    for (; i < N; ++i) {
        if (s[i] == ';') {
            if (i > i0 + 1) {
                res.push_back({ s + i0, i - i0 });
            }
            i0 = i+1;
        }
    }
    if (i > i0 + 1)
        res.push_back({ s + i0, i - i0 });
    return res;
}
