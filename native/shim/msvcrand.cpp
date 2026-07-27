// msvcrand.cpp - the MSVC CRT's rand()/srand(), reproduced exactly.
//
// Not a stylistic choice: panel layout, avatar placement, balloon shifts and
// title selection all consume rand(), the corpus goldens pin the resulting
// sequence, and the oracle records every draw in its seedLedger. Linking macOS's
// libc rand() instead would diverge on the first panel and every panel after.
//
// RULEBOOK 4 is the spec. The CRT implementation is:
//
//     holdrand = holdrand * 214013 + 2531011
//     return (holdrand >> 16) & 0x7fff
//
// with RAND_MAX 0x7fff. Two details are load-bearing:
//
//  * The state is 32 bits and must WRAP. uint32_t gives that by definition;
//    plain `long` here is 64-bit on arm64 macOS and would never wrap, silently
//    producing a different sequence from the second call onward.
//  * srand() stores the seed as-is with no pre-mixing, so srand(1) then rand()
//    yields 41 and srand(0) yields 38. Those two values are pinned in
//    port/test/core/numeric.test.ts and are the cheapest possible smoke test
//    that this file is correct.

#include <stdint.h>

namespace {
// Matches the CRT's default: rand() without a prior srand() behaves as srand(1).
uint32_t g_holdrand = 1;
}

extern "C" void msvc_srand(unsigned int seed) {
    g_holdrand = (uint32_t)seed;
}

extern "C" int msvc_rand(void) {
    g_holdrand = g_holdrand * 214013u + 2531011u;
    return (int)((g_holdrand >> 16) & 0x7fffu);
}
