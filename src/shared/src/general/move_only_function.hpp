#pragma once
#include "uwebsockets/MoveOnlyFunction.h"

template<typename T>
using MoveOnlyFunction = uWS::MoveOnlyFunction<T>;
