#include "../start_connection.hpp"
#include "connection.hpp"
#include "emscripten.h"
#include "eventloop/eventloop.hpp"
#include "global/emscripten_proxy.hpp"
#include "global/globals.hpp"
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <iostream>
using namespace std;

void start_connection(const WSConnectRequest& r)
{
    proxy_to_main_runtime([r]() {
        auto p { WSConnection::make_new(r) };
        if (!p) {
            cout << "Websocket connection failed" << endl;
            global().core->on_failed_connect(r, Error(ESTARTWEBSOCK));
        }
        p->start_read();
    });
}
