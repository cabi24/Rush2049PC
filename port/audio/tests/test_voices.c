/* More than 32 voices an SFD (the PC's [audio] voices, docs/port/AUDIO.md 10): SFDs of 48, 64 and 96 voice records
 * with the release mask's words for slots 32 and up after the records and the slots' last outputs after the state
 * block's 0x298 bytes. The sizes at 32 are the N64's; every voice of a full SFD is mixed, up to one at its last slot;
 * the list ends at the voice naming the output; a stopped voice past slot 31 folds into the base as one below it
 * does, by its own bit (bit 31 of the header's word is slot 31, bit 0 of the first word after the records slot 32);
 * slots below 32 keep the N64's place in the state block; and the N64 form (native order and big-endian) renders
 * the same as the native one. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define SUB MX_SUBFRAME
#define MAXV 96

static int16_t dc[1024];

static void dc_voice(PcMusyxVoice *v, const int16_t *src, int l, int r)
{
    memset(v, 0, sizeof *v);
    v->pitch = 0x1000;
    v->src[0].ptr1 = src;
    v->src[0].size1 = 200 * 2;
    v->u.pcm.count[0] = 198;
    v->end = 0x4000;
    v->env[0] = (int32_t)((uint32_t)(uint16_t)l << 16);
    v->env[1] = (int32_t)((uint32_t)(uint16_t)r << 16);
}

/* A DC source of value v at phase 0, times volume vol, as test_mix's dc_mix: each tap rounded and saturated, the
 * pairs and their sum saturated, the volume's product rounded. */
static int32_t dc_mix(int32_t v, int32_t vol)
{
    const int16_t *t = musyx_lut[0];
    int32_t r[4], x, y;
    for (int j = 0; j < 4; j++) {
        r[j] = (int32_t)(((int64_t)v * t[j] * 2 + 0x8000) >> 16);
        r[j] = r[j] > 32767 ? 32767 : r[j];
    }
    x = r[0] + r[1] > 32767 ? 32767 : r[0] + r[1];
    y = r[2] + r[3] > 32767 ? 32767 : r[2] + r[3];
    x = x + y > 32767 ? 32767 : x + y;
    return (int32_t)(((int64_t)x * vol * 2 + 0x8000) >> 16);
}

static int32_t decay(int32_t b)
{
    return (int32_t)(((int64_t)b * 0xF850) >> 16);
}

static int all(const int16_t *o, int16_t l, int16_t r)
{
    for (int i = 0; i < SUB; i++)
        if (o[2 * i] != l || o[2 * i + 1] != r)
            return 0;
    return 1;
}

/* SFD k of an array of n-voice SFDs, native form. */
static PcMusyxSfd *sfd_at(uint8_t *base, unsigned n, int k)
{
    return (PcMusyxSfd *)(base + (size_t)k * pc_audio_sfd_size(n, PC_AUDIO_FORM_NATIVE));
}

static PcMusyxVoice *voice_at(PcMusyxSfd *s, int i)
{
    return &s->voices[0] + i;   /* the records run on past the struct's 32 */
}

static uint32_t *mask_words(PcMusyxSfd *s, unsigned n)
{
    return (uint32_t *)((uint8_t *)s + MX_NATIVE_MASK_AT(n));
}

static void release(PcMusyxSfd *s, unsigned n, unsigned slot)
{
    if (slot < MX_VOICES)
        s->release_mask |= 1u << slot;
    else
        mask_words(s, n)[(slot - MX_VOICES) / 32] |= 1u << ((slot - MX_VOICES) % 32);
}

static int16_t last_left(const uint8_t *st, unsigned slot)
{
    int16_t v;
    memcpy(&v, slot < MX_VOICES ? st + MX_ST_LAST + slot * 8 : st + MX_STATE_SIZE + (slot - MX_VOICES) * 8, 2);
    return v;
}

static void put16(uint8_t *p, uint32_t v, int be)
{
    uint16_t x = (uint16_t)v;
    if (be) {
        p[0] = (uint8_t)(x >> 8);
        p[1] = (uint8_t)x;
    } else {
        memcpy(p, &x, 2);
    }
}

static void put32(uint8_t *p, uint32_t v, int be)
{
    if (be) {
        put16(p, v >> 16, 1);
        put16(p + 2, v, 1);
    } else {
        memcpy(p, &v, 4);
    }
}

/* One native SFD in the N64 form (the few fields these voices use), pointers as RDRAM addresses. */
static void sfd_to_n64(PcMusyxSfd *s, unsigned n, uint8_t *o, uint32_t state, int be)
{
    memset(o, 0, MX_SFD_SIZE_N(n));
    put16(o + MX_SFD_COUNT, s->count, be);
    put32(o + MX_SFD_RELEASE_MASK, s->release_mask, be);
    put32(o + MX_SFD_STATE, state, be);
    for (unsigned w = 0; w < MX_MASK_WORDS(n); w++)
        put32(o + MX_SFD_MASK_AT(n) + 4 * w, mask_words(s, n)[w], be);
    for (unsigned i = 0; i < n; i++) {
        const PcMusyxVoice *v = voice_at(s, (int)i);
        uint8_t *p = o + MX_SFD_VOICES + i * MX_V_SIZE;
        for (int k = 0; k < 4; k++)
            put32(p + MX_V_ENV + 4 * k, (uint32_t)v->env[k], be);
        put16(p + MX_V_PITCH, v->pitch, be);
        put32(p + MX_V_SRC + MX_CAT_PTR1, v->src[0].ptr1 ? rdram_addr(v->src[0].ptr1) : 0, be);
        put16(p + MX_V_SRC + MX_CAT_SIZE1, (uint16_t)v->src[0].size1, be);
        put16(p + MX_V_PCM_COUNT, (uint16_t)v->u.pcm.count[0], be);
        put32(p + MX_V_OUTPUT, v->output ? rdram_addr(v->output) : 0, be);
        put16(p + MX_V_END, v->end, be);
    }
}

int main(void)
{
    static const unsigned sizes[] = {48, 64, 96};
    PcMixerStats ms;

    musyx_lut_init();
    for (int k = 0; k < 1024; k++)
        dc[k] = 20000;

    /* 1. The sizes. 32 is the N64's SFD and state block; past it the records grow by one each and the mask by a
     * word a 32 slots, rounded to 8 bytes. */
    CHECK(pc_audio_sfd_size(32, PC_AUDIO_FORM_N64) == MX_SFD_SIZE, "N64 SFD %zu", pc_audio_sfd_size(32, 1));
    CHECK(pc_audio_sfd_size(32, PC_AUDIO_FORM_NATIVE) == sizeof(PcMusyxSfd), "native SFD %zu",
          pc_audio_sfd_size(32, 0));
    CHECK(pc_audio_sfd_size(8, PC_AUDIO_FORM_NATIVE) == sizeof(PcMusyxSfd), "fewer than 32: still 32 records");
    CHECK(MX_STATE_SIZE_N(32) == 0x298 && MX_STATE_SIZE_N(64) == 0x298 + 32 * 8, "state sizes");
    CHECK(MX_MASK_WORDS(32) == 0 && MX_MASK_WORDS(33) == 1 && MX_MASK_WORDS(64) == 1 && MX_MASK_WORDS(65) == 2 &&
          MX_MASK_WORDS(96) == 2, "mask words");
    CHECK(pc_audio_sfd_size(64, PC_AUDIO_FORM_N64) == 0x10 + 64 * 0x50 + 8, "N64 SFD of 64: %zu",
          pc_audio_sfd_size(64, 1));
    CHECK(pc_audio_sfd_size(96, PC_AUDIO_FORM_NATIVE) % 8 == 0 && pc_audio_sfd_size(48, PC_AUDIO_FORM_N64) % 8 == 0,
          "aligned");
    CHECK(pc_audio_sfd_size(300, PC_AUDIO_FORM_NATIVE) == pc_audio_sfd_size(MX_VOICES_MAX, PC_AUDIO_FORM_NATIVE),
          "capped at MX_VOICES_MAX");

    for (size_t si = 0; si < sizeof sizes / sizeof *sizes; si++) {
        unsigned n = sizes[si];
        size_t sz = pc_audio_sfd_size(n, PC_AUDIO_FORM_NATIVE);
        uint8_t *base = calloc(4, sz), *state = calloc(1, MX_STATE_SIZE_N(n));
        static int16_t out[4][2 * SUB];
        int32_t one = dc_mix(20000, 0x100), sum;
        pc_audio_set_sfd_voices(n);
        CHECK(pc_audio_sfd_voices() == n, "voices %u", pc_audio_sfd_voices());

        /* 2. A full SFD: every voice mixed, the last at slot n - 1 naming the output. */
        PcMusyxSfd *s = sfd_at(base, n, 0);
        s->state = state;
        s->count = (uint16_t)n;
        for (unsigned i = 0; i < n; i++)
            dc_voice(voice_at(s, (int)i), dc, 0x100, 0);
        voice_at(s, (int)n - 1)->output = out[0];
        pc_audio_run_task_form(base, 1, PC_AUDIO_FORM_NATIVE);
        sum = one * (int32_t)n;
        CHECK(all(out[0], (int16_t)sum, 0), "%u voices: %d, expected %d", n, out[0][0], sum);
        pc_audio_mixer_stats(&ms);
        CHECK(ms.voices_most == n && ms.slots == n, "%u voices: the mixer counted %u of %u", n, ms.voices_most,
              ms.slots);
        CHECK(last_left(state, n - 1) == one && last_left(state, 31) == one && last_left(state, 32) == one,
              "%u voices: last outputs %d %d %d", n, last_left(state, n - 1), last_left(state, 31),
              last_left(state, 32));

        /* 3. The list ends at the voice naming the output: past slot 40, louder voices aren't mixed. */
        memset(base, 0, 4 * sz);
        memset(state, 0, MX_STATE_SIZE_N(n));
        s = sfd_at(base, n, 0);
        s->state = state;
        for (unsigned i = 0; i < n; i++)
            dc_voice(voice_at(s, (int)i), dc, i <= 40 ? 0x100 : 0x4000, 0);
        voice_at(s, 40)->output = out[0];
        pc_audio_run_task_form(base, 1, PC_AUDIO_FORM_NATIVE);
        CHECK(all(out[0], (int16_t)(one * 41), 0), "%u voices, ended at 40: %d", n, out[0][0]);
        CHECK(last_left(state, 41) == 0, "slot 41 untouched: %d", last_left(state, 41));

        /* 4. Stopped voices fold into the base by their own bits. SFD 0: voices at slots 31, 32 and n - 1 with
         * their own volumes (the rest silent); SFD 1 releases slot 32 alone, SFD 2 slot n - 1 and slot 31: the base
         * holds each one's last output, decaying by 0xF850 / 2^16 a subframe. */
        memset(base, 0, 4 * sz);
        memset(state, 0, MX_STATE_SIZE_N(n));
        for (int k = 0; k < 3; k++) {
            sfd_at(base, n, k)->state = state;
        }
        s = sfd_at(base, n, 0);
        for (unsigned i = 0; i < n; i++)
            dc_voice(voice_at(s, (int)i), dc, i == 31 ? 0x1000 : i == 32 ? 0x2000 : i == n - 1 ? 0x0800 : 0, 0);
        voice_at(s, (int)n - 1)->output = out[0];
        dc_voice(voice_at(sfd_at(base, n, 1), 0), dc, 0, 0);
        voice_at(sfd_at(base, n, 1), 0)->src[0].size1 = 0;   /* no voices */
        voice_at(sfd_at(base, n, 1), 0)->output = out[1];
        release(sfd_at(base, n, 1), n, 32);
        voice_at(sfd_at(base, n, 2), 0)->output = out[2];
        release(sfd_at(base, n, 2), n, n - 1);
        release(sfd_at(base, n, 2), n, 31);
        pc_audio_run_task_form(base, 3, PC_AUDIO_FORM_NATIVE);
        {
            int32_t l31 = dc_mix(20000, 0x1000), l32 = dc_mix(20000, 0x2000), llast = dc_mix(20000, 0x0800);
            int32_t b = decay(l32);
            CHECK(all(out[0], (int16_t)(l31 + l32 + llast), 0), "%u: three voices %d", n, out[0][0]);
            CHECK(all(out[1], (int16_t)b, 0), "%u: slot 32 released: %d, expected %d", n, out[1][0], b);
            b = decay(b + llast + l31);
            CHECK(all(out[2], (int16_t)b, 0), "%u: slots 31 and %u released: %d, expected %d", n, n - 1, out[2][0],
                  b);
        }

        /* 5. The N64 form, native order and big-endian, of scene 4: the same outputs and state. */
        for (int be = 0; be < 2; be++) {
            static int16_t ref[3][2 * SUB];
            uint8_t ref_state[MX_STATE_SIZE_N(MAXV)];
            memcpy(ref, out, sizeof ref);
            memcpy(ref_state, state, MX_STATE_SIZE_N(n));
            rdram_reset();
            size_t nsz = pc_audio_sfd_size(n, PC_AUDIO_FORM_N64);
            uint8_t *img = rdram_alloc(3 * nsz), *st64 = rdram_alloc(MX_STATE_SIZE_N(n));
            int16_t *src = rdram_alloc(sizeof dc), *o64[3];
            memcpy(src, dc, sizeof dc);
            for (int k = 0; k < 3; k++)
                o64[k] = rdram_alloc(4 * SUB);
            for (int k = 0; k < 3; k++) {
                PcMusyxSfd *ns = sfd_at(base, n, k);
                for (unsigned i = 0; i < n; i++) {
                    PcMusyxVoice *v = voice_at(ns, (int)i);
                    if (v->src[0].ptr1)
                        v->src[0].ptr1 = src;
                    if (v->output)
                        v->output = o64[k];
                }
                sfd_to_n64(ns, n, img + k * nsz, rdram_addr(st64), be);
            }
            if (be)
                pc_mem_register_be(img, 3 * nsz);
            pc_audio_run_task_form(img, 3, PC_AUDIO_FORM_N64);
            if (be)
                pc_mem_unregister_be(img);
            int same = 1;
            for (int k = 0; k < 3; k++)
                same &= !memcmp(o64[k], ref[k], 4 * SUB);
            CHECK(same, "%u voices, N64 form %s: outputs differ (%d %d %d)", n, be ? "big-endian" : "native order",
                  o64[0][0], o64[1][0], o64[2][0]);
            CHECK(!memcmp(st64, ref_state, MX_STATE_SIZE_N(n)), "%u voices, N64 form %s: state differs", n,
                  be ? "big-endian" : "native order");
            /* back to the native scene's own buffers for the next form */
            for (int k = 0; k < 3; k++) {
                PcMusyxSfd *ns = sfd_at(base, n, k);
                for (unsigned i = 0; i < n; i++) {
                    PcMusyxVoice *v = voice_at(ns, (int)i);
                    if (v->src[0].ptr1)
                        v->src[0].ptr1 = dc;
                    if (v->output)
                        v->output = out[k];
                }
            }
        }
        free(base);
        free(state);
    }

    /* 6. Back at 32, a mask bit past 31 means nothing (no words follow the records): the N64's SFD. */
    pc_audio_set_sfd_voices(32);
    {
        uint8_t *base = calloc(1, sizeof(PcMusyxSfd)), state[MX_STATE_SIZE];
        static int16_t out[2 * SUB];
        PcMusyxSfd *s = (PcMusyxSfd *)base;
        memset(state, 0, sizeof state);
        s->state = state;
        for (int i = 0; i < 32; i++)
            dc_voice(&s->voices[i], dc, 0x100, 0);
        s->voices[31].output = out;
        pc_audio_run_task_form(base, 1, PC_AUDIO_FORM_NATIVE);
        CHECK(all(out, (int16_t)(dc_mix(20000, 0x100) * 32), 0), "32 voices at 32: %d", out[0]);
        free(base);
    }
    return test_finish("audio_voices");
}
