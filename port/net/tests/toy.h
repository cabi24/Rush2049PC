/* port/net/tests/toy.h: a tiny deterministic "game" for port/net's tests: a state that each tick mixes in every
 * port's input and a random number generator, and hashes itself. Two peers that apply the same inputs in the same
 * order have the same hash at every tick; one different input (or one changed bit) changes every later hash. */
#ifndef NET_TOY_H
#define NET_TOY_H

#include <stdint.h>

#include "net.h"

typedef struct Toy {
    uint64_t a, b;
    uint32_t rng, tick;
    int32_t x[NET_MAX_PORTS], y[NET_MAX_PORTS];
    uint32_t presses[NET_MAX_PORTS];
} Toy;

void toy_init(Toy *t);
void toy_step(Toy *t, const NetPad pads[NET_MAX_PORTS]);
uint32_t toy_hash(const Toy *t);
uint64_t toy_window_hash(const Toy *t);
/* Player `port`'s input sampled at its local retrace n: buttons that change at random retraces and stick sweeps,
 * a function of (seed, port, n) only. unique: buttons (and stick_y) carry n + 1 (the assignment tests). */
NetPad toy_input(uint64_t seed, int port, uint32_t n, int unique);

#endif
