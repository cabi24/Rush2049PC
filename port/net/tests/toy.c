/* port/net/tests/toy.c: the toy simulation (toy.h). */
#include "toy.h"

#include <string.h>

void toy_init(Toy *t)
{
    memset(t, 0, sizeof *t);
    t->a = 0x0123456789ABCDEFull;
    t->b = 0xFEDCBA9876543210ull;
    t->rng = 0x2049;
}

static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

void toy_step(Toy *t, const NetPad pads[NET_MAX_PORTS])
{
    for (int p = 0; p < NET_MAX_PORTS; p++) {
        t->rng = t->rng * 1103515245u + 12345u;   /* the game's LCG */
        t->x[p] += pads[p].stick_x;
        t->y[p] += pads[p].stick_y;
        if (pads[p].buttons & 0x8000)
            t->presses[p]++;
        uint64_t in = (uint64_t)pads[p].buttons | (uint64_t)(uint8_t)pads[p].stick_x << 16 |
                      (uint64_t)(uint8_t)pads[p].stick_y << 24 | (uint64_t)p << 32;
        t->a = rotl(t->a ^ in, 13) * 0x9E3779B97F4A7C15ull + t->rng;
        t->b += t->a ^ (t->b >> 7) ^ (uint64_t)(t->rng >> 16);
    }
    t->tick++;
}

static uint32_t fnv(uint32_t h, uint64_t v, int bytes)
{
    for (int i = 0; i < bytes; i++) {
        h ^= (uint8_t)(v >> (8 * i));
        h *= 16777619u;
    }
    return h;
}

uint32_t toy_hash(const Toy *t)
{
    uint32_t h = 2166136261u;
    h = fnv(h, t->a, 8);
    h = fnv(h, t->b, 8);
    h = fnv(h, t->rng, 4);
    h = fnv(h, t->tick, 4);
    for (int p = 0; p < NET_MAX_PORTS; p++) {
        h = fnv(h, (uint32_t)t->x[p], 4);
        h = fnv(h, (uint32_t)t->y[p], 4);
        h = fnv(h, t->presses[p], 4);
    }
    return h;
}

uint64_t toy_window_hash(const Toy *t)
{
    return (uint64_t)toy_hash(t) << 32 | (uint32_t)(t->a ^ t->b ^ t->tick);
}

static uint64_t mix(uint64_t z)
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

NetPad toy_input(uint64_t seed, int port, uint32_t n, int unique)
{
    NetPad p;
    uint64_t seg = mix(seed ^ (uint64_t)port << 56 ^ (n / 9));   /* buttons held for 9 retraces at a time */
    uint64_t r = mix(seed ^ (uint64_t)port << 48 ^ n);
    p.buttons = unique ? (uint16_t)(n + 1) : (uint16_t)(seg & 0xF3FF);
    p.stick_x = (int8_t)((int)((n * 3 + (seg >> 20)) % 160) - 80);   /* sweeps */
    p.stick_y = (int8_t)((int)((r >> 40) % 161) - 80);
    if (unique)   /* n + 1: 0 stays the idle input */
        p.stick_y = (int8_t)((n + 1) >> 16);
    return p;
}
