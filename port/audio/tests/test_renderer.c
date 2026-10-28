/* The game's renderer scheduling: which of the AI's ring buffers its retrace callback renders (func_80013DEC in
 * src/rom/lib_11640.c, docs/port/AUDIO.md 7). Each retrace it renders the buffers the AI has passed since the last,
 * at most a cap of them. Where that range wraps past the ring's end and the cap cuts its first part short, the N64
 * code starts the next retrace at the ring's start, and the buffers left at the end go unrendered: they play their
 * last turn's sound. The PC build (PC_RING_START) starts there only once the first part got to the end.
 *
 * The loops below are the function's, its globals as statics. The AI's ring position and the cap are made up: it
 * goes on by a few buffers a retrace, sometimes more than the cap, never a turn ahead of the renderer. The checks:
 * the PC loops render every buffer the AI passes once, in the ring's order, and catch up; the N64 loops don't. */
#include "test.h"

static unsigned short D_80038292;           /* the ring's length */
static volatile unsigned char D_80038288;   /* the AI's buffer */
static unsigned short D_80038360;           /* where the next render starts */
static unsigned short D_800382CE;           /* the cap: buffers rendered per retrace */
static int pc;                              /* the PC build's loops, else the N64's */

static long next_render;   /* the renders so far: the next is for the AI's buffer number next_render */
static long out_of_order;  /* renders not of that buffer */

#define PC_RING_START(index) (pc ? ((index) >= D_80038292 ? 0 : (index)) : 0)

static void func_80013D70(unsigned short index, unsigned short offset)
{
    (void)offset;
    if (index != next_render % D_80038292) {
        out_of_order++;
        next_render += (index + D_80038292 - next_render % D_80038292) % D_80038292;   /* goes on from there */
    }
    next_render++;
}

/* func_80013DEC's scheduling, with the renderer running (D_80038360 != 0xFFFF). */
static void retrace(void)
{
    unsigned short index;
    unsigned short offset;
    unsigned short count;
    unsigned short target;
    if (D_80038288 == 0) {
        target = D_80038292 - 1;
    } else {
        target = D_80038288 - 1;
    }
    if (D_80038360 != target) {
        offset = 0;
        count = 0;
        if (D_80038360 < target) {
            for (index = D_80038360; index < target && count < D_800382CE; offset += 192, count++) {
                func_80013D70(index++, offset);
            }
        } else {
            for (index = D_80038360; index < D_80038292 && count < D_800382CE; offset += 192, count++) {
                func_80013D70(index++, offset);
            }
            for (index = PC_RING_START(index); index < target && count < D_800382CE; offset += 192, count++) {
                func_80013D70(index++, offset);
            }
        }
        D_80038360 = index;
    }
}

/* One run: the ring's length and the cap; returns the renders out of order. *lag_ok says the renderer never fell a
 * turn behind (where its position can't tell) and caught up at the end. */
static long run(unsigned short len, unsigned short cap, unsigned seed, int retraces, int *lag_ok)
{
    long ai = len;   /* the AI's buffer number, from a turn on: D_80038288 is it in the ring */
    D_80038292 = len;
    D_800382CE = cap;
    D_80038288 = 0;
    D_80038360 = len - 1;   /* the resync: the next render is the AI's last buffer's next */
    next_render = len - 1;
    out_of_order = 0;
    *lag_ok = 1;
    for (int i = 0; i < retraces; i++) {
        long lag = ai - 1 - next_render;   /* buffers passed and not rendered */
        long step = test_rand(&seed) % (cap + 3);
        if (step > len - 2 - lag)
            step = len - 2 - lag > 0 ? len - 2 - lag : 0;
        ai += step;
        D_80038288 = (unsigned char)(ai % len);
        retrace();
        *lag_ok &= ai - 1 - next_render < len;
    }
    for (int i = 0; i < len; i++)   /* the AI waits: the renderer catches up */
        retrace();
    *lag_ok &= next_render == ai - 1;
    return out_of_order;
}

int main(void)
{
    static const unsigned short lens[] = {8, 12, 16, 24};   /* the ring's lengths (func_80010A40) */
    long n64_wrong = 0;

    /* The wrapping range cut short: from 5 of 8 to 1 with a cap of 2 renders 5 and 6, then 7, 0, ... */
    int ok;
    pc = 1;
    D_80038292 = 8;
    D_800382CE = 2;
    D_80038360 = 5;
    D_80038288 = 2;
    next_render = 5;
    out_of_order = 0;
    retrace();
    CHECK(D_80038360 == 7, "PC: a cut wrapping range goes on at 7, not %u", D_80038360);
    retrace();
    CHECK(D_80038360 == 1 && out_of_order == 0, "PC: then 7 and 0: at %u, %ld out of order", D_80038360,
          out_of_order);
    pc = 0;
    D_80038360 = 5;
    next_render = 5;
    retrace();
    CHECK(D_80038360 == 0, "N64: a cut wrapping range starts over at 0, not %u", D_80038360);

    for (unsigned li = 0; li < sizeof lens / sizeof lens[0]; li++)
        for (unsigned short cap = 1; cap <= 6; cap++)
            for (unsigned seed = 1; seed <= 20; seed++) {
                pc = 1;
                long wrong = run(lens[li], cap, seed, 2000, &ok);
                CHECK(!wrong && ok, "PC loops, ring %u, cap %u, seed %u: %ld renders out of order, kept up %d",
                      lens[li], cap, seed, wrong, ok);
                pc = 0;
                n64_wrong += run(lens[li], cap, seed, 2000, &ok);
            }
    CHECK(n64_wrong > 0, "the N64 loops pass buffers over in the same runs (%ld renders out of order)", n64_wrong);
    return test_finish("renderer");
}
