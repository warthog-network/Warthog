#include "connect_request.hpp"
#include "global/globals.hpp"
#include "transport/tcp/conman.hpp"

void TCPConnectRequest::connect()
{
    global().conman->connect(*this);
}

