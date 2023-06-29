#include"globals.hpp"
#include"asyncio/conman.hpp"

namespace{
    Global globalinstance;
}

const Global& global(){
    return globalinstance;
}

int init_config(int argc, char** argv){
    auto& conf = globalinstance.conf;
    if ( int  i= conf.init(argc,argv))
        return i;
    return 0;
};

const Config& config(){
    return globalinstance.conf;
}

void global_init(BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, Conman* pcm, Eventloop* pel){
    globalinstance.pbr=pbr;
    globalinstance.pps=pps;
    globalinstance.pcm=pcm;
    globalinstance.pcs=pcs;
    globalinstance.pel=pel;
};
