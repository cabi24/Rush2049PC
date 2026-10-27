/* The delay effect (func_80010E80's block): aux into the circular line, taps back into left and right,
 * feedback through the 4-tap filter with its history, and reads that wrap around the line. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define SUB MX_SUBFRAME
#define LINE (SUB * 8)

static PcMusyxSfd sfds[8];
static uint8_t state[MX_STATE_SIZE];
static int16_t out[8][2 * SUB];
static int16_t line[LINE];
static int16_t dc16k[1024];
static PcMusyxDelay d;

static void setup(int k, unsigned index, int aux_voice)
{
    memset(&sfds[k], 0, sizeof sfds[k]);
    sfds[k].state = state;
    sfds[k].delay = &d;
    sfds[k].delay_index = (uint16_t)index;
    if (aux_voice) {
        PcMusyxVoice *v = &sfds[k].voices[0];
        v->pitch = 0x1000;
        v->src[0].ptr1 = dc16k;
        v->src[0].size1 = 200 * 2;
        v->u.pcm.count[0] = 198;
        v->end = 0x4000;
        v->env[3] = 0x7FFF << 16;
        sfds[k].count = 1;
    }
    sfds[k].voices[0].output = out[k];
}

static int all(const int16_t *o, int16_t l, int16_t r)
{
    for (int i = 0; i < SUB; i++)
        if (o[2 * i] != l || o[2 * i + 1] != r)
            return 0;
    return 1;
}

int main(void)
{
    for (int k = 0; k < 1024; k++)
        dc16k[k] = 16384;

    /* 1. One tap, two subframes back, gain 0.5, no feedback: aux 16383 in subframe 0 is heard in subframe 2. */
    memset(&d, 0, sizeof d);
    d.buffer = line;
    d.length = LINE;
    d.taps = 1;
    d.tap_delay[0] = 2 * SUB;
    d.tap_gain[0] = 0x4000;
    d.filter[3] = 32767;
    memset(line, 0, sizeof line);
    memset(state, 0, sizeof state);
    setup(0, 0, 1);
    for (int k = 1; k < 4; k++)
        setup(k, (unsigned)k, 0);
    pc_audio_run_task_form(sfds, 4, PC_AUDIO_FORM_NATIVE);
    CHECK(line[0] == 16383 && line[SUB - 1] == 16383 && line[SUB] == 0, "line %d %d %d", line[0], line[SUB - 1], line[SUB]);
    CHECK(all(out[0], 0, 0) && all(out[1], 0, 0), "early echo %d %d", out[0][0], out[1][0]);
    CHECK(all(out[2], 8191, 8191), "echo %d %d", out[2][0], out[2][1]);
    CHECK(all(out[3], 0, 0), "after the echo %d", out[3][0]);

    /* 2. Feedback 0.5 through the pass-through filter: each repeat is half the last. */
    memset(line, 0, sizeof line);
    memset(state, 0, sizeof state);
    d.feedback = 0x4000;
    setup(0, 0, 1);
    for (int k = 1; k < 8; k++)
        setup(k, (unsigned)k, 0);
    pc_audio_run_task_form(sfds, 8, PC_AUDIO_FORM_NATIVE);
    {
        int h3 = (0x4000 * 32767) >> 15;   /* 16383 */
        int w2 = (16383 * 0x4000) >> 15;   /* 8191 */
        int l4 = (h3 * w2) >> 15, w4 = (l4 * 0x4000) >> 15;
        int l6 = (h3 * w4) >> 15, w6 = (l6 * 0x4000) >> 15;
        CHECK(all(out[2], (int16_t)w2, (int16_t)w2), "first echo %d", out[2][0]);
        CHECK(all(out[4], (int16_t)w4, (int16_t)w4), "second echo %d, expected %d", out[4][0], w4);
        CHECK(all(out[6], (int16_t)w6, (int16_t)w6), "third echo %d, expected %d", out[6][0], w6);
        CHECK(all(out[3], 0, 0) && all(out[5], 0, 0) && all(out[7], 0, 0), "odd subframes %d", out[5][0]);
    }

    /* 3. A tap that wraps: 100 samples back from position 0 reads line[1436..1535] then line[0..91]. */
    for (int k = 0; k < LINE; k++)
        line[k] = (int16_t)(k * 16);
    memset(state, 0, sizeof state);
    d.feedback = 0;
    d.tap_delay[0] = 100;
    d.tap_gain[0] = 0x7FFF;
    setup(0, 0, 0);
    pc_audio_run_task_form(sfds, 1, PC_AUDIO_FORM_NATIVE);
    {
        int bad = 0;
        for (int i = 0; i < SUB; i++) {
            int at = (LINE - 100 + i) % LINE;
            int16_t w = (int16_t)(((at * 16) * 0x7FFF) >> 15);
            bad += out[0][2 * i] != w || out[0][2 * i + 1] != w;
        }
        CHECK(!bad, "wrapping tap: %d samples wrong", bad);
    }

    /* 4. Filter history: {0, 0, 32767, 0} delays the feedback by a sample, carrying the last wet sample of
     * the previous subframe (in the state block) into the next. */
    for (int k = 0; k < LINE; k++)
        line[k] = (int16_t)(k * 8);
    memset(state, 0, sizeof state);
    d.feedback = 0x7FFF;
    d.tap_delay[0] = 4 * SUB;
    d.tap_gain[0] = 0x7FFF;
    memset(d.filter, 0, sizeof d.filter);
    d.filter[2] = 32767;
    setup(0, 4, 0);   /* pos 768: wet from line[0..191], written to line[768..959] */
    setup(1, 5, 0);   /* pos 960: wet from line[192..383] */
    pc_audio_run_task_form(sfds, 2, PC_AUDIO_FORM_NATIVE);
    {
        int h2 = (0x7FFF * 32767) >> 15, bad = 0;
        for (int k = 0; k < 2; k++) {
            for (int i = 0; i < SUB; i++) {
                int src = k * SUB + i - 1;   /* the wet sample one before */
                int prev_wet = src < 0 ? 0 : ((src * 8) * 0x7FFF) >> 15;
                int16_t want = (int16_t)((h2 * prev_wet) >> 15);
                bad += line[(4 + k) * SUB + i] != want;
            }
        }
        CHECK(!bad, "delayed feedback: %d samples wrong (line[960] %d)", bad, line[960]);
    }

    return test_finish("delay");
}
