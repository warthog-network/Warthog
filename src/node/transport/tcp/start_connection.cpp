#include "start_connection.hpp"
#include "global/globals.hpp"
#include "conman.hpp"
void start_connection(const TCPConnectRequest& r)
{
    global().conman->connect(r);
}
