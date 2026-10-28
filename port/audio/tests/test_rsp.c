/* The microcode's arithmetic, case by case (docs/port/AUDIO.md §8): the values each fix gives, worked out by hand
 * from its vector instructions, where the interpreter's first version (Q15 products floored, one saturation at the
 * end) gave others. Probe tables and pitch 0 hold every frame of a voice at one position and phase. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define SUB MX_SUBFRAME

static PcMusyxSfd sfds[2];
static uint8_t state[MX_STATE_SIZE];
static int16_t out[2][2 * SUB];

/* A PCM16 voice at pitch 0 over data[0..7]: every frame reads data[0..3] with table row `row`. */
static void still_voice(PcMusyxVoice *v, const int16_t *data, int row, int32_t left, int32_t right)
{
    memset(v, 0, sizeof *v);
    v->pitch = 0;
    v->pitch_frac = (uint16_t)(row << 10);
    v->src[0].ptr1 = data;
    v->src[0].size1 = 8 * 2;
    v->u.pcm.count[0] = 8;
    v->end = 0x4000;
    v->env[0] = left;
    v->env[1] = right;
}

static void run(int sfd_count, const int *voices)
{
    for (int k = 0; k < sfd_count; k++) {
        sfds[k].state = state;
        sfds[k].count = (uint16_t)voices[k];
        sfds[k].voices[voices[k] ? voices[k] - 1 : 0].output = out[k];
    }
    pc_audio_run_task_form(sfds, (size_t)sfd_count, PC_AUDIO_FORM_NATIVE);
}

static int all(const int16_t *o, int16_t l, int16_t r)
{
    for (int i = 0; i < SUB; i++)
        if (o[2 * i] != l || o[2 * i + 1] != r)
            return 0;
    return 1;
}

static void set_row(int row, int16_t a, int16_t b, int16_t c, int16_t d)
{
    musyx_lut[row][0] = a, musyx_lut[row][1] = b, musyx_lut[row][2] = c, musyx_lut[row][3] = d;
}

int main(void)
{
    static int16_t d3[8] = {3, 3, 3, 3, 3, 3, 3, 3}, dsat[8] = {32767, 32767, -32768, 0, 0, 0, 0, 0},
                   d1[8] = {1, 1, 1, 1}, dm1[8] = {-1, -1, -1, -1}, dmin[8] = {-32768, -32768, -32768, -32768},
                   d20k[8] = {20000, 20000, 20000, 20000};
    musyx_lut_init();

    /* 1. Each tap's product rounds half up (VMULF): 3 * 0.5 = 1.5 is 2 per tap, x = 4, and 4 * 0x7FFF / 32768 =
     * 3.9999 rounds to 4. Floored, x was 3 and the output 2. */
    set_row(0, 16384, 16384, 0, 0);
    memset(sfds, 0, sizeof sfds);
    memset(state, 0, sizeof state);
    still_voice(&sfds[0].voices[0], d3, 0, 0x7FFF << 16, 0);
    run(1, (const int[]){1});
    CHECK(all(out[0], 4, 0), "tap rounding: %d (expected 4)", out[0][0]);

    /* 2. The two pairs saturate before they add (VADD): 32767 * 0.99997 twice is 32766 + 32766, saturated to 32767;
     * -32768 * 0.99997 is -32767; x = 0. As one sum, x was 32765. */
    set_row(0, 32767, 32767, 32767, 0);
    memset(sfds, 0, sizeof sfds);
    still_voice(&sfds[0].voices[0], dsat, 0, 0x7FFF << 16, 0);
    run(1, (const int[]){1});
    CHECK(all(out[0], 0, 0), "pair saturation: %d (expected 0)", out[0][0]);

    /* 3. The volume's product rounds half up: x = 1 at 0.5 is 0.5, so 1; at -0.5, -0.5 rounds up to 0. x = -1 at 0.5
     * is 0. Floored, these were 0, -1 and -1. */
    set_row(1, 32767, 0, 0, 0);
    memset(sfds, 0, sizeof sfds);
    still_voice(&sfds[0].voices[0], d1, 1, 0x4000 << 16, (int32_t)0xC0000000u);
    still_voice(&sfds[1].voices[0], dm1, 1, 0x4000 << 16, 0);
    run(2, (const int[]){1, 1});
    CHECK(all(out[0], 1, 0), "volume rounding: %d %d (expected 1 0)", out[0][0], out[0][1]);
    CHECK(all(out[1], 0, 0), "volume rounding, negative: %d (expected 0)", out[1][0]);

    /* 4. The mix adds the product unsaturated and saturates once (VMULF then VMADH on its accumulator): voice 0
     * leaves -1; voice 1's x = -32768 at volume -32768 is +32768, and -1 + 32768 = 32767. Saturated first, 32766.
     * The slot's last output is the saturated product, 32767. */
    set_row(0, 16384, 16384, 0, 0);
    memset(sfds, 0, sizeof sfds);
    memset(state, 0, sizeof state);
    still_voice(&sfds[0].voices[0], dm1, 1, 0x7FFF << 16, 0);
    still_voice(&sfds[0].voices[1], dmin, 0, (int32_t)0x80000000u, 0);
    run(1, (const int[]){2});
    {
        int16_t last[4];
        memcpy(last, state + MX_ST_LAST + 8, sizeof last);
        CHECK(all(out[0], 32767, 0), "one saturation: %d (expected 32767)", out[0][0]);
        CHECK(last[0] == 32767, "last output %d", last[0]);
    }

    /* 5. A volume's high half saturates as it ramps (VADD): 0x7FF0 stepping 0x0010 a frame stays at 0x7FFF; it doesn't
     * wrap to -0x8000 (where the output would turn negative). x = 20000 * 0.99997 = 19999. */
    memset(sfds, 0, sizeof sfds);
    still_voice(&sfds[0].voices[0], d20k, 1, 0x7FF00000, 0);
    sfds[0].voices[0].env_step[0] = 0x00100000;
    run(1, (const int[]){1});
    {
        int bad = 0;
        for (int i = 1; i < SUB; i++)
            bad += out[0][2 * i] != 19998;   /* 19999 * 0x7FFF / 32768 = 19998.4 */
        CHECK(out[0][0] == 19989 && !bad, "saturating ramp: %d %d %d (expected 19989, then 19998)", out[0][0], out[0][2],
              out[0][2 * 191]);
    }

    /* 6. ADPCM: the predictor's sum is the accumulator's 32 bits (VMADH), so it wraps. Book 0 all -32768, the first
     * two samples -32768, every residual -8 << 12 at shift 0. Sample 2: -2^26 + 2^31 fits, >> 11 saturates to
     * 32767. Sample 3 adds 2^30 more: 3 * 2^30 - 2^26 wraps to -2^30 - 2^26, and saturates to -32768 (in 64 bits it
     * was 32767). */
    {
        int16_t book[128], dst[32];
        uint8_t blk[40];
        for (int i = 0; i < 128; i++)
            book[i] = i < 16 ? -32768 : 0;
        memset(blk, 0x88, sizeof blk);
        blk[0] = 0x80, blk[1] = 0x00, blk[2] = 0x80, blk[3] = 0x00;
        blk[8] = 0x00;   /* book 0, shift 0 */
        musyx_adpcm_decode(dst, blk, sizeof blk, book, 1, 0);
        CHECK(dst[0] == -32768 && dst[1] == -32768 && dst[2] == 32767 && dst[3] == -32768, "ADPCM wrap: %d %d %d %d",
              dst[0], dst[1], dst[2], dst[3]);
    }

    /* 7. The delay effect rounds as well: a line of 1 at gain 0.5 is 0.5, so 1 (floored, 0). A line of 30000 at
     * gain 0x7FFF is 29999.6, so 29999; the filter's 32767 at feedback 0.5 is 16383.5, so 16384; and the feedback
     * 29999 * 16384 / 32768 = 14999.5 rounds to 15000 (with 16383 floored, 14998). */
    {
        static int16_t line[2 * SUB];
        static PcMusyxDelay d;
        memset(&d, 0, sizeof d);
        d.buffer = line;
        d.length = 2 * SUB;
        d.taps = 1;
        d.tap_delay[0] = SUB;
        d.tap_gain[0] = 0x4000;
        d.feedback = 0x4000;
        d.filter[3] = 32767;
        for (int i = 0; i < 2 * SUB; i++)
            line[i] = 1;
        memset(sfds, 0, sizeof sfds);
        memset(state, 0, sizeof state);
        sfds[0].delay = &d;
        sfds[0].delay_index = 1;   /* writes line[192..383], reads line[0..191] */
        run(1, (const int[]){0});
        CHECK(all(out[0], 1, 1), "tap gain rounding: %d (expected 1)", out[0][0]);
        for (int i = 0; i < 2 * SUB; i++)
            line[i] = 30000;
        d.tap_gain[0] = 0x7FFF;
        memset(state, 0, sizeof state);
        run(1, (const int[]){0});
        CHECK(all(out[0], 29999, 29999) && line[SUB] == 15000 && line[2 * SUB - 1] == 15000,
              "feedback rounding: %d, line %d (expected 29999, 15000)", out[0][0], line[SUB]);
    }

    return test_finish("rsp");
}
