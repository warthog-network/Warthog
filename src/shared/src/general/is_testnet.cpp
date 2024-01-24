#include"is_testnet.hpp"
namespace{
    bool isTestnet{false};
}
bool is_testnet(){
    return isTestnet;
};
void enable_testnet(){
    isTestnet=true;
};
