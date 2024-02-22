#pragma once
#include "block/block.hpp"
#include<map>
#include<string>


class StratumCoordinator;
class StratumSession
{
    friend class StratumCoordinator;
public:

};

class StratumCoordinator
{
public:
    void new_prev_hash(const Hash& h);
private:
    std::map<uint64_t,StratumSession> sessions;

    std::optional<Hash> prevHash;
    std::map<std::string,Block> blocks;
};
