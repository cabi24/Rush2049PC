/* PCM16 voices: where the segments land, skip, pitch stepping, end and restart points, byte order, split
 * source ranges. A probe table (every phase {0, 0.5, 0, 0}) and volume 0.5 make each output sample exactly
 * a quarter of input sample [pos + 1]. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

static PcMusyxSfd sfd;
static uint8_t state[MX_STATE_SIZE];
static int16_t out[2 * MX_SUBFRAME];

static void run_voice(const PcMusyxVoice *v)
{
    memset(&sfd, 0, sizeof sfd);
    sfd.count = 1;
    sfd.state = state;
    sfd.voices[0] = *v;
    sfd.voices[0].output = out;
    pc_audio_run_task_form(&sfd, 1, PC_AUDIO_FORM_NATIVE);
}

/* A main-segment voice at input position a, unity volume 0.5 on the left. */
static void main_voice(PcMusyxVoice *v, const int16_t *data, unsigned a, unsigned count, uint16_t pitch,
                       uint16_t frac)
{
    unsigned skip = a & 3, n = (count + skip + 3) & ~3u;
    memset(v, 0, sizeof *v);
    v->env[0] = 0x4000 << 16;
    v->pitch = pitch;
    v->pitch_frac = frac;
    v->src[0].ptr1 = data + (a & ~3u);
    v->src[0].size1 = (int16_t)(n * 2);
    v->u.pcm.count[0] = (int16_t)count;
    v->skip[0] = (uint8_t)skip;
    v->end = 0x4000;
}

int main(void)
{
    static int16_t ramp[4096], noise[4096], loopdata[256];
    static uint8_t ramp_be[8192];
    PcMusyxVoice v;
    unsigned seed = 7;

    for (int k = 0; k < 4096; k++) {
        ramp[k] = (int16_t)(4 * k);
        ramp_be[2 * k] = (uint8_t)((uint16_t)ramp[k] >> 8);
        ramp_be[2 * k + 1] = (uint8_t)ramp[k];
        noise[k] = (int16_t)(test_rand(&seed) - 16384);
    }

    /* 1. The real table, against the closed form: input index (frac + i * step) >> 16, phase bits 15..10. */
    musyx_lut_init();
    for (int t = 0; t < 3; t++) {
        static const uint16_t pitches[3] = {0x1357, 0x0A00, 0x1FFF};
        static const uint16_t fracs[3] = {0x1234, 0xFFFF, 0};
        unsigned a = 301 + t, count = ((pitches[t] * 192 + 0xFFF) >> 12) + 6;
        main_voice(&v, noise, a, count, pitches[t], fracs[t]);
        run_voice(&v);
        int bad = 0;
        for (int i = 0; i < MX_SUBFRAME; i++) {
            uint32_t p = fracs[t] + (uint32_t)i * (pitches[t] << 4);
            const int16_t *tap = musyx_lut[(p >> 10) & 63];
            int64_t s = 0;
            for (int j = 0; j < 4; j++)
                s += noise[a + (p >> 16) + j] * tap[j];
            int32_t x = s >> 15 > 32767 ? 32767 : s >> 15 < -32768 ? -32768 : (int32_t)(s >> 15);
            bad += out[2 * i] != (int16_t)((x * 0x4000) >> 15) || out[2 * i + 1] != 0;
        }
        CHECK(!bad, "pitch %04x: %d samples differ from the closed form", pitches[t], bad);
    }

    for (int r = 0; r < 64; r++) {
        musyx_lut[r][0] = 0;
        musyx_lut[r][1] = 16384;
        musyx_lut[r][2] = 0;
        musyx_lut[r][3] = 0;
    }

    /* 2. Unity pitch from each alignment: output i is input a + i + 1. */
    for (unsigned a = 100; a < 104; a++) {
        main_voice(&v, ramp, a, 198, 0x1000, 0);
        run_voice(&v);
        int bad = 0;
        for (int i = 0; i < MX_SUBFRAME; i++)
            bad += out[2 * i] != (int16_t)(a + i + 1);
        CHECK(!bad, "skip %u: %d samples wrong (first %d)", a & 3, bad, out[0]);
    }

    /* 3. Pitch 2.0 and 0.5 (with a half-sample start). */
    main_voice(&v, ramp, 205, 390, 0x2000, 0);
    run_voice(&v);
    {
        int bad = 0;
        for (int i = 0; i < MX_SUBFRAME; i++)
            bad += out[2 * i] != (int16_t)(205 + 2 * i + 1);
        CHECK(!bad, "pitch 2.0: %d wrong", bad);
    }
    main_voice(&v, ramp, 205, 102, 0x0800, 0x8000);
    run_voice(&v);
    {
        int bad = 0;
        for (int i = 0; i < MX_SUBFRAME; i++)
            bad += out[2 * i] != (int16_t)(205 + (i + 1) / 2 + 1);
        CHECK(!bad, "pitch 0.5: %d wrong", bad);
    }

    /* 4. End and restart: the main segment (12 samples at the buffer's end) runs to its end point 10, then
     * continues in the loop segment (at the buffer's start) at 2; or restarts inside the main segment at 4. */
    for (int k = 0; k < 256; k++)
        loopdata[k] = (int16_t)(4 * (3000 + k));
    main_voice(&v, ramp, 1000, 12, 0x1000, 0);
    v.src[1].ptr1 = loopdata;
    v.src[1].size1 = 200 * 2;
    v.u.pcm.count[1] = 200;
    v.end = 10;
    v.restart = 0x8000 | 2;
    run_voice(&v);
    {
        int bad = 0;
        for (int i = 0; i < MX_SUBFRAME; i++)
            bad += out[2 * i] != (int16_t)(i < 10 ? 1000 + i + 1 : 3000 + (i - 10) + 3);
        CHECK(!bad, "restart in the loop segment: %d wrong (out[10] %d)", bad, out[20]);
    }
    v.src[1].ptr1 = NULL;
    v.src[1].size1 = 0;
    v.u.pcm.count[1] = 0;
    v.restart = 4;
    run_voice(&v);
    {
        int bad = 0;
        for (int i = 0; i < MX_SUBFRAME; i++)
            bad += out[2 * i] != (int16_t)(i < 10 ? 1000 + i + 1 : 1005 + (i - 10) % 6);
        CHECK(!bad, "restart in the main segment: %d wrong", bad);
    }
    /* start (0x4E) adds to the first sample's offset. */
    main_voice(&v, ramp, 1000, 198 + 7, 0x1000, 0);
    v.start = 7;
    run_voice(&v);
    CHECK(out[0] == 1008 && out[2 * 191] == 1008 + 191, "start offset: %d", out[0]);

    /* 5. Byte order: the same samples big-endian in a registered range. */
    main_voice(&v, (const int16_t *)ramp_be, 513, 198, 0x1000, 0);
    pc_mem_register_be(ramp_be, sizeof ramp_be);
    run_voice(&v);
    pc_mem_unregister_be(ramp_be);
    {
        int bad = 0;
        for (int i = 0; i < MX_SUBFRAME; i++)
            bad += out[2 * i] != (int16_t)(513 + i + 1);
        CHECK(!bad, "big-endian source: %d wrong (first %d)", bad, out[0]);
    }

    /* 6. A source split across two ranges (a cache boundary) reads as one. */
    main_voice(&v, ramp, 700, 198, 0x1000, 0);
    {
        int total = v.src[0].size1;
        v.src[0].size1 = 104;
        v.src[0].ptr2 = ramp + 700 + 52;
        v.src[0].size2 = (int16_t)(total - 104);
        run_voice(&v);
        int bad = 0;
        for (int i = 0; i < MX_SUBFRAME; i++)
            bad += out[2 * i] != (int16_t)(700 + i + 1);
        CHECK(!bad, "split source: %d wrong", bad);
    }

    return test_finish("pcm");
}
