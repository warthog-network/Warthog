#include "../start_connection.hpp"
#include "global/globals.hpp"
#include "ws_conman.hpp"

void start_connection(const WSConnectRequest& r){
    global().wsconman->connect(r);
}
