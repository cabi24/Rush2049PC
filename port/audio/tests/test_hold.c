/* Pushes held for their render (audio_out.c): the game's ring of AI buffers, rendered by its retrace callback and
 * pushed by the AI as the device's queue needs them, in made-up timings.
 *
 * The model is the game's: R buffers pushed in ring order, each render for one push (block u fills buffer u % R
 * with u's sound, for push u), renders in order and at most R - 2 slots past the last push (the renderer only
 * rewrites buffers the AI has passed), stalls, and restarts that jump to the ring's front. The device pulls a
 * buffer's length at a time through the manual backend, at the game's rate (no conversion). Every block of output
 * is either silence or one render's sound, so the checks are on the sequence of renders heard. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define R 8
#define N 192
#define PROBE 100   /* a frame past the 32-frame fade-in */

static int16_t ring[R][2 * N];
static uint64_t renders, pushes, pulls;   /* made so far: renders are for pushes 0, 1, ... */
static long heard[20000];
static int channels_ok = 1;                /* every block's right channel is its left negated */                 /* per pulled block: the render heard + 1, or 0 for silence */

static void fill(uint64_t u)
{
    int16_t v = (int16_t)(1 + u % 30000);
    for (int i = 0; i < N; i++) {
        ring[u % R][2 * i] = v;
        ring[u % R][2 * i + 1] = (int16_t)-v;
    }
}

static void render(uint64_t u)
{
    fill(u);
    pc_audio_note_rendered(ring[u % R]);
}

static void push(void)
{
    pc_audio_push(ring[pushes % R], N);
    pushes++;
}

static long pull(void)
{
    static int16_t out[2 * N];
    pc_audio_pull(out, N);
    long id = out[2 * PROBE];
    channels_ok &= out[2 * PROBE + 1] == -id;
    heard[pulls++] = id;
    return id;
}

static void reset(void)
{
    pc_audio_init("manual", NULL);
    pc_audio_set_frequency(22050);
    memset(ring, 0, sizeof ring);
    renders = pushes = pulls = 0;
}

/* The game's start: a turn of pushes of the zeroed ring before the first render, played. */
static void first_turn(void)
{
    for (int i = 0; i < R; i++) {
        push();
        pull();
    }
    renders = R;
}

/* The heard sequence from block `from`: in order, never early, never twice. Returns the silent blocks. */
static int check_heard(uint64_t from, const char *what)
{
    long last = 0;
    int silent = 0, early = 0, order = 0;
    for (uint64_t i = from; i < pulls; i++) {
        long id = heard[i];
        if (!id) {
            silent++;
            continue;
        }
        early += (uint64_t)(id - 1) > i;   /* block i is push i: render i is the newest it may play */
        order += id <= last;
        last = id;
    }
    CHECK(!early && !order, "%s: %d blocks early, %d out of order or again", what, early, order);
    return silent;
}

int main(void)
{
    PcAudioStats st;

    /* 1. Rendered before pushed, as on the N64: every push plays at once, its own render. */
    reset();
    first_turn();
    for (int i = 0; i < 6 * R; i++) {
        render(renders++);
        push();
    }
    while (pulls < pushes)
        pull();
    pc_audio_get_stats(&st);
    int exact = 1;
    for (uint64_t i = R; i < pulls; i++)
        exact &= heard[i] == (long)i + 1;
    CHECK(exact && st.waited == 0, "renders ahead: every block its own render (waited %llu)",
          (unsigned long long)st.waited);

    /* 2. Pushed first, rendered before the device needs it: held, then played as if on time. */
    reset();
    first_turn();
    for (int i = 0; i < 2 * R; i++) {
        render(renders++);
        push();
    }
    for (int i = 0; i < 2 * R; i++) {
        push();
        CHECK(pc_audio_queued_frames() == (pushes - pulls) * N, "held frames count as queued");
        render(renders++);
        if (i % 2)
            pull();
    }
    while (pulls < pushes)
        pull();
    pc_audio_get_stats(&st);
    exact = 1;
    for (uint64_t i = R; i < pulls; i++)
        exact &= heard[i] == (long)i + 1;
    CHECK(exact && st.waited == 2 * R && st.rescued == 2 * R && st.missed == 0,
          "late renders in time: exact %d, waited %llu, rescued %llu, missed %llu", exact,
          (unsigned long long)st.waited, (unsigned long long)st.rescued, (unsigned long long)st.missed);

    /* 3a. The device needs a push before its render comes: silence in its place, not the ring's last turn. The late
     * render never plays: a turn later it would be out of place, and the next turn's replaces it. */
    reset();
    first_turn();
    for (int i = 0; i < 2 * R; i++) {
        render(renders++);
        push();
        pull();
    }
    uint64_t late = pushes;
    push();
    CHECK(pull() == 0, "a push the device needed before its render: silence");
    render(renders++);   /* late: for push `late` */
    for (int i = 0; i < R - 1; i++) {
        render(renders++);
        push();
        pull();
    }
    push();              /* late's buffer again, needed before its render */
    CHECK(pull() == 0, "the late render a turn later would be out of order: silence");
    render(renders++);
    for (int i = 0; i < R; i++) {
        render(renders++);
        push();
        pull();
    }
    pc_audio_get_stats(&st);
    CHECK(st.missed == 2 && st.skipped == 2 && st.played_late == 0, "missed %llu skipped %llu late %llu",
          (unsigned long long)st.missed, (unsigned long long)st.skipped, (unsigned long long)st.played_late);
    CHECK(check_heard(R, "one late render") == 2, "two silent blocks");

    /* 3b. The renderer falls three behind and stays there: after a turn of silence, every buffer plays its
     * previous turn's render, in order (late by a turn, not out of place); caught up, it plays on time again. */
    reset();
    first_turn();
    for (int i = 0; i < 2 * R; i++) {
        render(renders++);
        push();
        pull();
    }
    uint64_t lag_from = pushes;
    for (int i = 0; i < 3; i++) {
        push();
        pull();
    }
    for (int i = 0; i < 3 * R; i++) {
        render(renders++);
        push();
        pull();
    }
    int chain = 1;
    for (uint64_t i = lag_from + R; i < pulls; i++)
        chain &= heard[i] == (long)(i - R) + 1;
    pc_audio_get_stats(&st);
    CHECK(chain && st.played_late >= 2 * R, "a turn late, in order: %d (played late %llu)", chain,
          (unsigned long long)st.played_late);
    while (renders < pushes + 2)
        render(renders++);   /* catching up */
    for (int i = 0; i < R; i++) {
        render(renders++);
        push();
        pull();
    }
    CHECK(heard[pulls - 1] == (long)pulls, "caught up: on time again");
    check_heard(R, "a renderer behind");

    /* 4. Random timings within the game's rules. Renders in order, at most R - 3 past the pushes; stalls (no renders
     * for a while, then on in order); restarts as the game's (func_80014140, func_800140F8): the renderer goes on to
     * the ring's front, stops, and some pushes later starts at the new front. A renderer slower than the AI at times.
     * What is heard is in order, never early, never twice; once the renderer is ahead again everything plays. */
    unsigned seed = 2049;
    for (int run = 0; run < 60; run++) {
        reset();
        first_turn();
        int depth = 2 + test_rand(&seed) % 6;            /* the AI's lead over the device, in buffers */
        int render_weight = 15 + test_rand(&seed) % 50;  /* against the pushes' 25 */
        int pause = 0, restart = 0;                      /* pushes left without renders; a restart after them */
        for (int step = 0; step < 3000; step++) {
            int r = test_rand(&seed) % 100;
            int phase = step < 2200;
            if (phase && !pause && r < 1) {
                pause = 1 + test_rand(&seed) % (3 * R);   /* a stall */
                continue;
            }
            if (phase && !pause && r < 2) {
                while (renders + 3 <= pushes + R)          /* the stop renders up to the front */
                    render(renders++);
                pause = test_rand(&seed) % (2 * R);
                restart = pause > 0;                       /* none: it starts again where it stopped */
                continue;
            }
            if (r < render_weight) {
                if (!pause && renders + 3 <= pushes + R)
                    render(renders++);
            } else if (r < render_weight + 25) {
                if (pushes - pulls < (uint64_t)depth) {
                    push();
                    if (pause && !--pause && restart) {
                        if (renders < pushes - 2 + R)
                            renders = pushes - 2 + R;
                        restart = 0;
                    }
                }
            } else if (pulls < pushes) {
                pull();
            }
        }
        /* settle: the device plays what is queued; then the renderer two ahead for three turns */
        while (pulls < pushes)
            pull();
        for (int i = 0; i < 3 * R; i++) {
            while (renders + 3 <= pushes + R && renders < pushes + 2)
                render(renders++);
            push();
            pull();
        }
        char what[32];
        snprintf(what, sizeof what, "random run %d", run);
        check_heard(R, what);
        int tail_exact = 1;
        for (uint64_t i = pulls - 2 * R; i < pulls; i++)
            tail_exact &= heard[i] == (long)i + 1;
        CHECK(tail_exact, "%s: the renderer ahead again, but not every block its own render", what);
    }

    /* 4b. The same buffer rendered twice running (the game does at loads): the later render plays, for that push,
     * and the renders kept for the next pushes stay. */
    reset();
    first_turn();
    for (int i = 0; i < 2 * R; i++) {
        render(renders++);
        push();
        pull();
    }
    render(renders);       /* the next push's buffer, then again with other sound */
    render(renders + 1);
    for (int i = 0; i < 2 * N; i++)
        ring[(renders + 1) % R][i] = (int16_t)(i % 2 ? -7777 : 7777);
    pc_audio_note_rendered(ring[(renders + 1) % R]);
    pc_audio_get_stats(&st);
    uint64_t rematches = st.rematched;
    renders += 2;
    push();
    push();
    long first = pull(), second = pull();
    pc_audio_get_stats(&st);
    CHECK(first == (long)pushes - 1 && second == 7777 && st.rematched == rematches && st.skipped == 1,
          "a render again: %ld %ld, rematched %llu, skipped %llu", first, second, (unsigned long long)st.rematched,
          (unsigned long long)st.skipped);

    /* 5. Before any render (the tests' and the boot's pushes), and pushes longer than an AI buffer: as pushed. */
    {
        static int16_t big[2 * 3000];
        reset();
        for (int i = 0; i < 3000; i++)
            big[2 * i] = big[2 * i + 1] = 7;
        fill(0);
        for (int i = 0; i < 3; i++)
            pc_audio_push(ring[0], N);   /* the same buffer again and again, never rendered */
        pc_audio_get_stats(&st);
        CHECK(st.waited == 0 && pc_audio_queued_frames() == 3 * N, "untracked pushes play as pushed");
        pc_audio_push(big, 3000);
        CHECK(pc_audio_queued_frames() == 3 * N + 3000, "a long push queues after them");
    }
    pc_audio_shutdown();
    CHECK(channels_ok, "a block's channels stay together");
    return test_finish("hold");
}
