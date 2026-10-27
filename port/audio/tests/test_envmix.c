/* The envelope mixer: per-frame volume ramps on all four channels, signed volumes, ramps joined across
 * subframes by the game's builder, saturation, and the last-sample record per voice slot. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define SUB MX_SUBFRAME

static PcMusyxSfd sfd;
static uint8_t state[MX_STATE_SIZE];
static int16_t out[2 * SUB];
static int16_t dc16k[1024], dc30k[1024];

/* A unity-pitch voice over a DC source. */
static void dc_voice(PcMusyxVoice *v, const int16_t *src)
{
    memset(v, 0, sizeof *v);
    v->pitch = 0x1000;
    v->src[0].ptr1 = src;
    v->src[0].size1 = 200 * 2;
    v->u.pcm.count[0] = 198;
    v->end = 0x4000;
}

static void run(int voices)
{
    sfd.count = (uint16_t)voices;
    sfd.state = state;
    sfd.voices[voices - 1].output = out;
    pc_audio_run_task_form(&sfd, 1, PC_AUDIO_FORM_NATIVE);
}

int main(void)
{
    for (int k = 0; k < 1024; k++) {
        dc16k[k] = 16384;
        dc30k[k] = 30000;
    }

    /* 1. Left ramps up from 0 to 0x7FFF, right ramps down from 0x7FFF to -0x8000, over one subframe. */
    memset(&sfd, 0, sizeof sfd);
    dc_voice(&sfd.voices[0], dc16k);
    sfd.voices[0].env[0] = 0;
    sfd.voices[0].env_step[0] = (int32_t)(0x7FFF0000 / SUB);
    sfd.voices[0].env[1] = 0x7FFF << 16;
    sfd.voices[0].env_step[1] = (int32_t)(((int64_t)-0x8000 * 65536 - (int64_t)0x7FFF * 65536) / SUB);
    run(1);
    {
        int bad = 0, mono = 1;
        for (int i = 0; i < SUB; i++) {
            int32_t vl = (int32_t)((uint32_t)i * (uint32_t)sfd.voices[0].env_step[0]) >> 16;
            int32_t vr = (int32_t)((uint32_t)sfd.voices[0].env[1] + (uint32_t)i * (uint32_t)sfd.voices[0].env_step[1]) >> 16;
            bad += out[2 * i] != (int16_t)((16384 * vl) >> 15) || out[2 * i + 1] != (int16_t)((16384 * vr) >> 15);
            if (i)
                mono &= out[2 * i] >= out[2 * i - 2] && out[2 * i + 1] <= out[2 * i - 1];
        }
        CHECK(!bad, "ramps: %d samples differ", bad);
        CHECK(mono, "ramps not monotonic");
        CHECK(out[0] == 0 && out[2 * 191] > 16200 && out[2 * 191 + 1] < -16200, "ramp ends: %d %d", out[2 * 191],
              out[2 * 191 + 1]);
    }

    /* 2. Ramps joined across subframes as the game builds them: 0 -> 0x7FFF -> 0x2000 -> -0x4000. */
    {
        static int16_t joined[4 * SUB];
        static const int16_t targets[4] = {0x7FFF, 0x2000, -0x4000, -0x4000};
        TbVoice tv = {(const uint8_t *)dc16k, 0, 1000, 0, 0, 0, 0x1000, {0}, 0};
        for (int k = 0; k < 4; k++) {
            const int16_t t[4] = {targets[k], 0, 0, 0};
            memset(&sfd, 0, sizeof sfd);
            tv.position = 0;   /* keep reading the DC source */
            tb_voice(&tv, &sfd.voices[0], t);
            run(1);
            for (int i = 0; i < SUB; i++)
                joined[k * SUB + i] = out[2 * i];
        }
        int worst = 0;
        for (int i = 1; i < 4 * SUB; i++) {
            int d = abs(joined[i] - joined[i - 1]);
            worst = d > worst ? d : worst;
        }
        /* the steepest ramp, 0 -> 0x7FFF at half scale, moves 0x7FFF / 2 / 192 = 85.3 per frame */
        CHECK(worst <= 86, "largest step between frames %d", worst);
        CHECK(joined[SUB] >= 16380 && joined[2 * SUB] >= 4090 && joined[2 * SUB] <= 4100 &&
                  joined[3 * SUB] == -8192 && joined[4 * SUB - 1] == -8192,
              "subframe starts %d %d %d", joined[SUB], joined[2 * SUB], joined[3 * SUB]);
    }

    /* 3. Saturation, and 4. each slot's last output per channel. */
    memset(&sfd, 0, sizeof sfd);
    for (int s = 0; s < 3; s++) {
        dc_voice(&sfd.voices[s], dc30k);
        sfd.voices[s].env[0] = 0x7FFF << 16;
        sfd.voices[s].env[1] = (int32_t)0x80000000u;
        sfd.voices[s].env[2] = 0x1000 << 16;
        sfd.voices[s].env[3] = (0x2000 + s) << 16;
    }
    memset(state, 0, sizeof state);
    run(3);
    {
        int bad = 0;
        for (int i = 0; i < SUB; i++)
            bad += out[2 * i] != 32767 || out[2 * i + 1] != -32768;
        CHECK(!bad, "saturation: %d samples (first %d %d)", bad, out[0], out[1]);
        for (int s = 0; s < 3; s++) {
            int16_t last[4];
            memcpy(last, state + MX_ST_LAST + s * 8, sizeof last);
            CHECK(last[0] == 29999 && last[1] == -30000 && last[2] == 3750 && last[3] == (30000 * (0x2000 + s)) >> 15,
                  "slot %d last %d %d %d %d", s, last[0], last[1], last[2], last[3]);
        }
    }

    /* 5. Aux alone doesn't reach the output without the delay effect. */
    memset(&sfd, 0, sizeof sfd);
    memset(state, 0, sizeof state);
    dc_voice(&sfd.voices[0], dc16k);
    sfd.voices[0].env[3] = 0x7FFF << 16;
    run(1);
    {
        int nz = 0;
        for (int i = 0; i < 2 * SUB; i++)
            nz += out[i] != 0;
        CHECK(!nz, "aux leaked into the output: %d samples", nz);
    }

    return test_finish("envmix");
}
