/* The game's renderer scheduling: which of the AI's ring buffers its retrace callback renders (sound_hw_service_frame in
 * src/rom/sound/hardware.c, docs/port/AUDIO.md 7). Each retrace it renders the buffers the AI has passed since the last,
 * at most a cap of them. Where that range wraps past the ring's end and the cap cuts its first part short, the N64
 * code starts the next retrace at the ring's start, and the buffers left at the end go unrendered: they play their
 * last turn's sound. The PC build (PC_RING_START) starts there only once the first part got to the end.
 *
 * The loops below are the function's, its globals as statics. The AI's ring position and the cap are made up: it
 * goes on by a few buffers a retrace, sometimes more than the cap, never a turn ahead of the renderer. The checks:
 * the PC loops render every buffer the AI passes once, in the ring's order, and catch up; the N64 loops don't. */
#include "test.h"

static unsigned short sound_output_buffer_count;           /* the ring's length */
static volatile unsigned char sound_output_index;   /* the AI's buffer */
static unsigned short sound_render_index;           /* where the next render starts */
static unsigned short sound_sfds_per_task;           /* the cap: buffers rendered per retrace */
static int pc;                              /* the PC build's loops, else the N64's */

static long next_render;   /* the renders so far: the next is for the AI's buffer number next_render */
static long out_of_order;  /* renders not of that buffer */

#define PC_RING_START(index) (pc ? ((index) >= sound_output_buffer_count ? 0 : (index)) : 0)

static void sound_hw_render_subframe(unsigned short index, unsigned short offset)
{
    (void)offset;
    if (index != next_render % sound_output_buffer_count) {
        out_of_order++;
        next_render += (index + sound_output_buffer_count - next_render % sound_output_buffer_count) % sound_output_buffer_count;   /* goes on from there */
    }
    next_render++;
}

/* sound_hw_service_frame's scheduling, with the renderer running (sound_render_index != 0xFFFF). */
static void retrace(void)
{
    unsigned short index;
    unsigned short offset;
    unsigned short count;
    unsigned short target;
    if (sound_output_index == 0) {
        target = sound_output_buffer_count - 1;
    } else {
        target = sound_output_index - 1;
    }
    if (sound_render_index != target) {
        offset = 0;
        count = 0;
        if (sound_render_index < target) {
            for (index = sound_render_index; index < target && count < sound_sfds_per_task; offset += 192, count++) {
                sound_hw_render_subframe(index++, offset);
            }
        } else {
            for (index = sound_render_index; index < sound_output_buffer_count && count < sound_sfds_per_task; offset += 192, count++) {
                sound_hw_render_subframe(index++, offset);
            }
            for (index = PC_RING_START(index); index < target && count < sound_sfds_per_task; offset += 192, count++) {
                sound_hw_render_subframe(index++, offset);
            }
        }
        sound_render_index = index;
    }
}

/* One run: the ring's length and the cap; returns the renders out of order. *lag_ok says the renderer never fell a
 * turn behind (where its position can't tell) and caught up at the end. */
static long run(unsigned short len, unsigned short cap, unsigned seed, int retraces, int *lag_ok)
{
    long ai = len;   /* the AI's buffer number, from a turn on: sound_output_index is it in the ring */
    sound_output_buffer_count = len;
    sound_sfds_per_task = cap;
    sound_output_index = 0;
    sound_render_index = len - 1;   /* the resync: the next render is the AI's last buffer's next */
    next_render = len - 1;
    out_of_order = 0;
    *lag_ok = 1;
    for (int i = 0; i < retraces; i++) {
        long lag = ai - 1 - next_render;   /* buffers passed and not rendered */
        long step = test_rand(&seed) % (cap + 3);
        if (step > len - 2 - lag)
            step = len - 2 - lag > 0 ? len - 2 - lag : 0;
        ai += step;
        sound_output_index = (unsigned char)(ai % len);
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
    static const unsigned short lens[] = {8, 12, 16, 24};   /* the ring's lengths (sound_hw_run_output_thread) */
    long n64_wrong = 0;

    /* The wrapping range cut short: from 5 of 8 to 1 with a cap of 2 renders 5 and 6, then 7, 0, ... */
    int ok;
    pc = 1;
    sound_output_buffer_count = 8;
    sound_sfds_per_task = 2;
    sound_render_index = 5;
    sound_output_index = 2;
    next_render = 5;
    out_of_order = 0;
    retrace();
    CHECK(sound_render_index == 7, "PC: a cut wrapping range goes on at 7, not %u", sound_render_index);
    retrace();
    CHECK(sound_render_index == 1 && out_of_order == 0, "PC: then 7 and 0: at %u, %ld out of order", sound_render_index,
          out_of_order);
    pc = 0;
    sound_render_index = 5;
    next_render = 5;
    retrace();
    CHECK(sound_render_index == 0, "N64: a cut wrapping range starts over at 0, not %u", sound_render_index);

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
