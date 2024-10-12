#pragma once
#include "emscripten.h"
#include <string>
extern "C" {
EMSCRIPTEN_KEEPALIVE
void virtual_get_request(int id, char* url);
EMSCRIPTEN_KEEPALIVE
void virtual_post_request(int id, char* url, char* postdata);
}
void virtual_endpoint_initialize();
