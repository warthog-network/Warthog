#pragma once
#include "block/chain/header_chain.hpp"
class Eventloop;
namespace BlockDownload {

class Attorney {
    public:
    Attorney(Eventloop& eventloop)
        : eventloop(eventloop) {};
    void clear_blockdownload() ;
    auto set_stage_headers(Headerchain&& newheaders);
    const Headerchain& headers() const;
    auto connections() const;

    private:
    Eventloop& eventloop;
};
}
