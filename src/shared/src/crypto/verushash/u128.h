#pragma once
#if defined(__arm__) || defined(__aarch64__)
#include "crypto/sse2neon.h"
#else
#include "immintrin.h"
#endif
#ifdef _WIN32
typedef unsigned long long u64;
#else
typedef unsigned long u64;
#endif
typedef __m128i u128;
