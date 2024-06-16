#pragma once

#ifndef DISABLE_LIBUV
#include "transport/tcp/connect_request.hpp"
using ConnectRequest = TCPConnectRequest;
#else
#include "transport/ws/browser/connect_request.hpp"
using ConnectRequest = WSBrowserConnectRequest;
#endif
