#pragma once
#include "block/chain/header_chain.hpp"
class Eventloop;
namespace BlockDownload {

class Attorney {
    public:
    Attorney(Eventloop& eventloop)
        : eventloop(eventloop) {};
    void clear_blockdownload() ;
    auto update_blockdownlad(Headerchain&& newheaders);
    const Headerchain& headers() const;
    auto connections() const;

    private:
    Eventloop& eventloop;
};
}
