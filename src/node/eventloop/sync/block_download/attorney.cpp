#include "attorney.hpp"
#include "../../eventloop.hpp"
#include "eventloop/address_manager/address_manager_impl.hpp"


namespace BlockDownload {
void Attorney::clear_blockdownload() {
    eventloop.chains.stage_clear();
    for (auto c : eventloop.connections.initialized()) {
        c->chain.on_stage_set(Height(0));
    }
}
const Headerchain& Attorney::headers() const{

    return eventloop.chains.stage_headers();
}
}
