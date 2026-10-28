/* Subframe mixing: empty SFDs, the voice list's end, stopped voices fading through the base volumes, the
 * delayed antiphase surround channel, and state carried between tasks. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define SUB MX_SUBFRAME

static PcMusyxSfd sfds[8];
static uint8_t state[MX_STATE_SIZE];
static int16_t out[8][2 * SUB];
static int16_t dc20k[1024], dc16k[1024];

static void dc_voice(PcMusyxVoice *v, const int16_t *src, int l, int r, int s, int a)
{
    memset(v, 0, sizeof *v);
    v->pitch = 0x1000;
    v->src[0].ptr1 = src;
    v->src[0].size1 = 200 * 2;
    v->u.pcm.count[0] = 198;
    v->end = 0x4000;
    v->env[0] = (int32_t)((uint32_t)(uint16_t)l << 16);
    v->env[1] = (int32_t)((uint32_t)(uint16_t)r << 16);
    v->env[2] = (int32_t)((uint32_t)(uint16_t)s << 16);
    v->env[3] = (int32_t)((uint32_t)(uint16_t)a << 16);
}

/* SFD k: its voices (up to 3, from v), output out[k]. */
static void sfd_setup(int k, const PcMusyxVoice *v, int voices, uint32_t mask)
{
    memset(&sfds[k], 0, sizeof sfds[k]);
    sfds[k].state = state;
    sfds[k].release_mask = mask;
    sfds[k].count = (uint16_t)voices;
    for (int i = 0; i < voices; i++)
        sfds[k].voices[i] = v[i];
    sfds[k].voices[voices ? voices - 1 : 0].output = out[k];
    for (int i = 0; i < 2 * SUB; i++)
        out[k][i] = 0x5555;   /* must be overwritten */
}

static int all(const int16_t *o, int16_t l, int16_t r)
{
    for (int i = 0; i < SUB; i++)
        if (o[2 * i] != l || o[2 * i + 1] != r)
            return 0;
    return 1;
}

/* A DC source of value v through the resampler at phase 0, times volume vol: the microcode's rounding (each tap's
 * product rounded half up and saturated, the pairs and their sum saturated; the volume's product rounded). */
static int32_t dc_mix(int32_t v, int32_t vol)
{
    const int16_t *t = musyx_lut[0];
    int32_t r[4], x;
    for (int j = 0; j < 4; j++) {
        r[j] = (int32_t)(((int64_t)v * t[j] * 2 + 0x8000) >> 16);
        r[j] = r[j] > 32767 ? 32767 : r[j];
    }
    x = r[0] + r[1] > 32767 ? 32767 : r[0] + r[1] < -32768 ? -32768 : r[0] + r[1];
    int32_t y = r[2] + r[3] > 32767 ? 32767 : r[2] + r[3] < -32768 ? -32768 : r[2] + r[3];
    x = x + y > 32767 ? 32767 : x + y < -32768 ? -32768 : x + y;
    return (int32_t)(((int64_t)x * vol * 2 + 0x8000) >> 16);
}

static int32_t decay(int32_t b)
{
    return (int32_t)(((int64_t)b * 0xF850) >> 16);
}

int main(void)
{
    PcMusyxVoice v[3];
    for (int k = 0; k < 1024; k++) {
        dc20k[k] = 20000;
        dc16k[k] = 16384;
    }

    /* 1. An SFD without voices writes silence (its output pointer is in voice 0). */
    memset(state, 0, sizeof state);
    sfd_setup(0, v, 0, 0);
    pc_audio_run_task_form(sfds, 1, PC_AUDIO_FORM_NATIVE);
    CHECK(all(out[0], 0, 0), "empty SFD output %d %d", out[0][0], out[0][1]);

    /* An empty SFD leaves the slots' last samples alone: slot 0's record survives it and fades later. */
    memset(state, 0, sizeof state);
    dc_voice(&v[0], dc20k, 0x7FFF, 0, 0, 0);
    sfd_setup(0, v, 1, 0);
    sfd_setup(1, v, 0, 0);
    sfd_setup(2, v, 0, 1);
    pc_audio_run_task_form(sfds, 3, PC_AUDIO_FORM_NATIVE);
    CHECK(all(out[2], (int16_t)decay(19999), 0), "slot 0 after an empty SFD: %d", out[2][0]);

    /* 2. The voice list ends at the voice naming the output: voice 2 isn't mixed. */
    memset(state, 0, sizeof state);
    dc_voice(&v[0], dc20k, 0x1000, 0, 0, 0);
    dc_voice(&v[1], dc20k, 0x2000, 0, 0, 0);
    sfd_setup(0, v, 2, 0);
    dc_voice(&sfds[0].voices[2], dc20k, 0x7FFF, 0x7FFF, 0, 0);
    pc_audio_run_task_form(sfds, 1, PC_AUDIO_FORM_NATIVE);
    CHECK(all(out[0], (20000 * 0x1000 >> 15) + (20000 * 0x2000 >> 15), 0), "two voices %d", out[0][0]);

    /* 3. A stopped voice fades: slot 2's last output enters the base volumes and decays by 0xF850 / 2^16 per
     * subframe, through four SFDs of one task and two more in the next. */
    memset(state, 0, sizeof state);
    dc_voice(&v[0], dc20k, 0x0100, 0x0100, 0, 0);
    dc_voice(&v[1], dc20k, 0x0100, 0x0100, 0, 0);
    dc_voice(&v[2], dc20k, 0x7FFF, 0x4000, 0, 0);
    sfd_setup(0, v, 3, 0);
    sfd_setup(1, v, 0, 1u << 2);
    sfd_setup(2, v, 0, 0);
    sfd_setup(3, v, 0, 0);
    pc_audio_run_task_form(sfds, 4, PC_AUDIO_FORM_NATIVE);
    {
        int16_t l0 = (int16_t)(2 * (20000 * 0x100 >> 15) + 19999), r0 = (int16_t)(2 * (20000 * 0x100 >> 15) + 10000);
        int32_t bl = decay(19999), br = decay(10000);
        CHECK(all(out[0], l0, r0), "voices: %d %d", out[0][0], out[0][1]);
        CHECK(all(out[1], (int16_t)bl, (int16_t)br), "released: %d %d, expected %d %d", out[1][0], out[1][1], bl, br);
        bl = decay(bl), br = decay(br);
        CHECK(all(out[2], (int16_t)bl, (int16_t)br), "decay 2: %d", out[2][0]);
        bl = decay(bl), br = decay(br);
        CHECK(all(out[3], (int16_t)bl, (int16_t)br), "decay 3: %d", out[3][0]);
        sfd_setup(0, v, 0, 0);
        sfd_setup(1, v, 0, 0);
        pc_audio_run_task_form(sfds, 2, PC_AUDIO_FORM_NATIVE);
        bl = decay(bl), br = decay(br);
        CHECK(all(out[0], (int16_t)bl, (int16_t)br), "next task: %d, expected %d", out[0][0], bl);
        bl = decay(bl), br = decay(br);
        CHECK(all(out[1], (int16_t)bl, (int16_t)br), "next task 2: %d", out[1][0]);
    }

    /* 4. Surround is mixed one subframe late, into left and (inverted) right. */
    memset(state, 0, sizeof state);
    dc_voice(&v[0], dc16k, 0, 0, 0x4000, 0);
    sfd_setup(0, v, 1, 0);
    sfd_setup(1, v, 1, 0);
    sfd_setup(2, v, 0, 0);
    sfd_setup(3, v, 0, 0);
    pc_audio_run_task_form(sfds, 2, PC_AUDIO_FORM_NATIVE);
    pc_audio_run_task_form(sfds + 2, 2, PC_AUDIO_FORM_NATIVE);
    int16_t sv = (int16_t)dc_mix(16384, 0x4000);   /* about 8192 */
    CHECK(all(out[0], 0, 0), "surround, first subframe: %d %d", out[0][0], out[0][1]);
    CHECK(all(out[1], sv, (int16_t)-sv), "surround, second: %d %d", out[1][0], out[1][1]);
    CHECK(all(out[2], sv, (int16_t)-sv), "surround after the voice ends, across tasks: %d %d", out[2][0], out[2][1]);
    CHECK(all(out[3], 0, 0), "surround gone: %d %d", out[3][0], out[3][1]);

    /* A stopped voice's surround carry goes the same way. */
    memset(state, 0, sizeof state);
    sfd_setup(0, v, 1, 0);
    sfd_setup(1, v, 0, 1);
    pc_audio_run_task_form(sfds, 2, PC_AUDIO_FORM_NATIVE);
    {
        int32_t bs = decay(sv);
        CHECK(all(out[1], (int16_t)(sv + bs), (int16_t)(-sv - bs)), "surround base: %d %d (base %d)", out[1][0],
              out[1][1], bs);
    }

    /* 5. One task of six SFDs equals tasks of two and four. */
    {
        static int16_t a[6][2 * SUB];
        static uint8_t st_a[MX_STATE_SIZE];
        for (int pass = 0; pass < 2; pass++) {
            TbVoice t0 = {(const uint8_t *)dc20k, 0, 1000, 0, 0, 0, 0x1234, {0}, 1};
            memset(state, 0, sizeof state);
            for (int k = 0; k < 6; k++) {
                const int16_t tg[4] = {(int16_t)(0x1000 * k), (int16_t)(-0x900 * k), 0x0800, 0x0400};
                memset(&sfds[k], 0, sizeof sfds[k]);
                sfds[k].state = state;
                t0.position = 10;
                if (k < 4) {
                    tb_voice(&t0, &sfds[k].voices[0], tg);
                    sfds[k].count = 1;
                }
                sfds[k].release_mask = k == 4 ? 1 : 0;
                sfds[k].voices[0].output = out[k];
            }
            if (pass == 0) {
                pc_audio_run_task_form(sfds, 6, PC_AUDIO_FORM_NATIVE);
                memcpy(a, out, sizeof a);
                memcpy(st_a, state, sizeof st_a);
            } else {
                pc_audio_run_task_form(sfds, 2, PC_AUDIO_FORM_NATIVE);
                pc_audio_run_task_form(sfds + 2, 4, PC_AUDIO_FORM_NATIVE);
                CHECK(!memcmp(a, out, sizeof a), "split tasks differ");
                CHECK(!memcmp(st_a, state, sizeof st_a), "split tasks leave a different state");
            }
        }
    }

    return test_finish("mix");
}
