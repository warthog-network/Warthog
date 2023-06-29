#pragma once
#include "config/config.hpp"
class BatchRegistry;
class PeerServer;
class ChainServer;
class Eventloop;
class Conman;

struct Global{
    ChainServer* pcs;
    PeerServer* pps;
    Conman* pcm;
    Eventloop* pel;
    BatchRegistry* pbr;
    Config conf;
};

const Global& global();
const Config& config();
int init_config(int argc, char** argv);
void global_init(BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, Conman* pcm, Eventloop* pel);

