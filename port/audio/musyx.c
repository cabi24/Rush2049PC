/* port/audio/musyx.c: the audio task interpreter, MusyX v1 (musyx_abi.h for the records).
 *
 * Per task: load the carried state (base volumes, the delayed surround subframe, the feedback FIR history),
 * then per SFD:
 *   1. base: add the last output of each voice slot in the release mask, then decay all four by 0xF850/2^16
 *   2. init: aux = base aux; left = surround(previous) + base surround; right = -(same); surround = 0
 *   3. voices, until one names an output buffer: load the source (PCM16 or ADPCM) into a 0x200-sample
 *      buffer, main segment at its end and loop segment at its start; resample with the 4-tap table, handling
 *      the end and restart points; mix into left, right, surround and aux with a per-frame volume ramp
 *   4. delay effect, if any: sum up to 8 taps of the circular line into a wet subframe, add it to left and
 *      right, and write aux + FIR(wet) * feedback back into the line
 *   5. output: interleave left + base left and right + base right, native s16
 * and store the state back. The arithmetic is the microcode's, lane by lane (docs/port/AUDIO.md §8): products
 * rounded half up as VMULF rounds them, saturation where its VADD and VMADH saturate, positions and volumes in
 * 8-frame vectors with a saturating high half, the ADPCM predictor's sum in 32 bits. Checked bit for bit against
 * the microcode translated by RSPRecomp, on the game's tasks (RUSH2049_AUDIO_CAPTURE) and on random ones. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L   /* clock_gettime, for the mixer's CPU time */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#include "musyx.h"
#include "pc_audio.h"

#define BUF 0x200          /* the microcode's decoded-sample buffer, in samples */
#define BUF_PAD 8          /* reads just past it see zeros */
#define RAW_MAX 720        /* ADPCM bytes per segment: 16 frames from either half of a block, plus slack */
#define SUB MX_SUBFRAME

static inline int16_t sat16(int64_t v)
{
    return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v;
}

/* ---------------------------------------------------------------- the vector unit's arithmetic, lane by lane */

/* VMULF as the accumulator holds it: a * b / 32768, rounded half up, not saturated. */
static inline int32_t mulf_acc(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * b * 2 + 0x8000) >> 16);
}

/* VMULF's result. */
static inline int16_t mulf(int32_t a, int32_t b)
{
    return sat16(mulf_acc(a, b));
}

/* A 16.16 value the way the microcode keeps one in two vectors: a u16 low half and an s16 high half. */
typedef struct {
    uint16_t lo;
    int16_t hi;
} Split;

/* VADDC then VADD: the low halves' carry goes into the high half, which saturates. */
static inline void split_add(Split *s, uint32_t inc)
{
    uint32_t lo = (uint32_t)s->lo + (inc & 0xFFFF);
    s->lo = (uint16_t)lo;
    s->hi = sat16((int32_t)s->hi + (int16_t)(inc >> 16) + (int32_t)(lo >> 16));
}

/* value + k * step for lane k, as the microcode spreads a ramp over its 8 lanes (VMUDN, VMADH, VADDC, VADD). */
static inline Split split_lane(uint32_t value, uint32_t step, int k)
{
    uint32_t low = (step & 0xFFFF) * (uint32_t)k;
    Split s = {(uint16_t)low, sat16((int64_t)(int16_t)(step >> 16) * k + (int64_t)(low >> 16))};
    split_add(&s, value);
    return s;
}

/* The 4-tap filter: each product rounded (VMULF), the pairs and then their sum saturated (VADD). */
static inline int16_t tap4(const int16_t *x, const int16_t t[4])
{
    return sat16((int32_t)sat16(mulf(x[0], t[0]) + mulf(x[1], t[1])) + sat16(mulf(x[2], t[2]) + mulf(x[3], t[3])));
}

static inline uint16_t ld16(const uint8_t *p, int be)
{
    uint16_t v;
    if (be)
        return (uint16_t)(p[0] << 8 | p[1]);
    memcpy(&v, p, 2);
    return v;
}

static inline uint32_t ld32(const uint8_t *p, int be)
{
    uint32_t v;
    if (be)
        return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    memcpy(&v, p, 4);
    return v;
}

/* ---------------------------------------------------------------- records, decoded */

typedef struct {
    const uint8_t *ptr[2];
    unsigned size[2];   /* bytes */
} Cat;

typedef struct {
    int32_t env[4], step[4];
    uint16_t frac, pitch;
    Cat src[2];
    uint8_t frames[2], skip[2];
    const uint8_t *book;
    uint16_t count[2];
    int16_t *output;
    uint16_t end, restart, start;
} Voice;

typedef struct {
    uint16_t delay_index;
    uint32_t mask;
    uint8_t *state;
    const PcMusyxDelay *delay;
    const PcMusyxVoice *voices;
    const uint8_t *mask_ext;   /* slots 32 and up: MX_MASK_WORDS(n) u32 words after the voice records, or NULL */
    unsigned n;                /* voice records */
} Sfd;

/* The voice records an SFD holds (pc_audio_set_sfd_voices): the N64's 32 unless the game gives more. */
static unsigned sfd_voices = MX_VOICES;

void pc_audio_set_sfd_voices(unsigned n)
{
    sfd_voices = n < MX_VOICES ? MX_VOICES : n > MX_VOICES_MAX ? MX_VOICES_MAX : n;
}

unsigned pc_audio_sfd_voices(void)
{
    return sfd_voices;
}

size_t pc_audio_sfd_size(unsigned n)
{
    n = n < MX_VOICES ? MX_VOICES : n > MX_VOICES_MAX ? MX_VOICES_MAX : n;
    return MX_NATIVE_SFD_SIZE(n);
}

typedef struct {
    int16_t *buffer;
    uint32_t length;
    unsigned taps;
    int16_t feedback;
    uint32_t tap_delay[8];
    int16_t tap_gain[8];
    int16_t filter[4];
} Delay;

/* The records are the game's own structs (hardware.c's SfdBlock, SfdVoice and SoundDelayState, which musyx_abi.h
 * spells as PcMusyxSfd, PcMusyxVoice and PcMusyxDelay), in host byte order, with host pointers. */
static void read_sfd(Sfd *s, const uint8_t *p, unsigned voices)
{
    const PcMusyxSfd *n = (const PcMusyxSfd *)p;
    s->n = voices;
    s->delay_index = n->delay_index;
    s->mask = n->release_mask;
    s->state = n->state;
    s->delay = n->delay;
    s->voices = n->voices;
    s->mask_ext = voices > MX_VOICES ? p + MX_NATIVE_MASK_AT(voices) : NULL;
}

/* Bit `slot` of the release mask: the header's word for slots 0-31, the words after the voice records past them. */
static int released(const Sfd *s, unsigned slot)
{
    uint32_t word;
    if (slot < MX_VOICES)
        return (int)(s->mask >> slot & 1);
    if (!s->mask_ext || slot >= s->n)
        return 0;
    slot -= MX_VOICES;
    memcpy(&word, s->mask_ext + 4 * (slot / 32), 4);
    return (int)(word >> (slot % 32) & 1);
}

/* A slot's last output in the state block: the N64's s16[32][4] at 0, then past the block's 0x298 bytes. */
static uint8_t *last_at(uint8_t *st, unsigned slot)
{
    return slot < MX_VOICES ? st + MX_ST_LAST + slot * 8 : st + MX_STATE_SIZE + (slot - MX_VOICES) * 8;
}

/* Bytes in voice 0's main range: zero means the SFD has no voices. */
static unsigned first_size(const Sfd *s)
{
    return (uint16_t)s->voices[0].src[0].size1;
}

static int16_t *voice_output(const Sfd *s, int i)
{
    return s->voices[i].output;
}

static void read_voice(Voice *v, const Sfd *s, int i)
{
    const PcMusyxVoice *n = &s->voices[i];
    memcpy(v->env, n->env, sizeof v->env);
    memcpy(v->step, n->env_step, sizeof v->step);
    v->frac = n->pitch_frac;
    v->pitch = n->pitch;
    for (int seg = 0; seg < 2; seg++) {
        v->src[seg].ptr[0] = n->src[seg].ptr1;
        v->src[seg].ptr[1] = n->src[seg].ptr2;
        v->src[seg].size[0] = (uint16_t)n->src[seg].size1;
        v->src[seg].size[1] = (uint16_t)n->src[seg].size2;
    }
    memcpy(v->frames, n->frames, 2);
    memcpy(v->skip, n->skip, 2);
    v->book = v->frames[0] ? n->u.book : NULL;
    v->count[0] = (uint16_t)n->u.pcm.count[0];
    v->count[1] = (uint16_t)n->u.pcm.count[1];
    v->output = n->output;
    v->end = n->end;
    v->restart = n->restart;
    v->start = n->start;
}

static int read_delay(Delay *d, const PcMusyxDelay *n)
{
    if (!n)
        return 0;
    d->buffer = n->buffer;
    d->length = n->length;
    d->taps = n->taps;
    d->feedback = n->feedback;
    memcpy(d->tap_delay, n->tap_delay, sizeof d->tap_delay);
    memcpy(d->tap_gain, n->tap_gain, sizeof d->tap_gain);
    memcpy(d->filter, n->filter, sizeof d->filter);
    if (d->taps > 8)
        d->taps = 8;
    return d->buffer && d->length;
}

/* ---------------------------------------------------------------- source loading */

/* A voice's two ranges, concatenated, as bytes. Returns bytes stored; missing data stays zero. */
static size_t cat_bytes(uint8_t *dst, size_t cap, const Cat *c)
{
    size_t n = 0;
    memset(dst, 0, cap);
    for (int r = 0; r < 2; r++) {
        size_t len = c->size[r];
        if (len > cap - n)
            len = cap - n;
        if (c->ptr[r] && len)
            memcpy(dst + n, c->ptr[r], len);
        n += len;
    }
    return n;
}

/* The same as s16 samples: big-endian inside a pc_mem_is_be() range, native elsewhere. */
static void cat_s16(int16_t *dst, size_t cap, const Cat *c)
{
    size_t n = 0;
    for (int r = 0; r < 2 && n < cap; r++) {
        size_t len = c->size[r] / 2;
        const uint8_t *p = c->ptr[r];
        if (len > cap - n)
            len = cap - n;
        if (!p) {
            n += len;
            continue;
        }
        if (pc_mem_is_be(p)) {
            for (size_t i = 0; i < len; i++)
                dst[n + i] = (int16_t)(p[2 * i] << 8 | p[2 * i + 1]);
        } else {
            memcpy(dst + n, p, len * 2);
        }
        n += len;
    }
}

void musyx_adpcm_decode(int16_t *dst, const uint8_t *src, size_t src_len, const int16_t book[128],
                        unsigned frames, unsigned skip)
{
    static const uint8_t group_start[4] = {2, 8, 16, 24};
    static const uint8_t group_len[4] = {6, 8, 8, 8};
    unsigned first = (skip >> 5) & 1;

    for (unsigned f = 0; f < frames; f++, dst += 32) {
        unsigned g = f + first;                  /* frame number from the first block */
        size_t blk = (size_t)(g >> 1) * 40;
        size_t raw = blk + (g & 1) * 4;          /* two raw samples */
        size_t nib = blk + 8 + (g & 1) * 16;     /* header byte, then 15 bytes of nibbles */
        uint8_t b[20];
        for (int i = 0; i < 4; i++)
            b[i] = raw + i < src_len ? src[raw + i] : 0;
        for (int i = 0; i < 16; i++)
            b[4 + i] = nib + i < src_len ? src[nib + i] : 0;

        const int16_t *c0 = book + ((b[4] >> 4) & 7) * 16;   /* weights of the sample two back */
        const int16_t *c1 = c0 + 8;                         /* one back, and the in-group impulse response */
        unsigned shift = b[4] & 15;
        int16_t res[32];
        for (int k = 2; k < 32; k++) {
            uint8_t byte = b[4 + k / 2];
            unsigned nibble = k & 1 ? byte & 15 : byte >> 4;
            res[k] = (int16_t)((int16_t)(nibble << 12) >> shift);
        }

        dst[0] = (int16_t)(b[0] << 8 | b[1]);
        dst[1] = (int16_t)(b[2] << 8 | b[3]);
        for (int grp = 0; grp < 4; grp++) {
            int s0 = group_start[grp];
            int32_t p2 = dst[s0 - 2], p1 = dst[s0 - 1];
            for (int i = 0; i < group_len[grp]; i++) {
                /* the accumulator's 32 bits above its low 16 (VMADH), then >> 11 saturated (VMUDN, VMADH) */
                uint32_t acc = (uint32_t)(res[s0 + i] * 2048) + (uint32_t)(c0[i] * p2) + (uint32_t)(c1[i] * p1);
                for (int j = 0; j < i; j++)
                    acc += (uint32_t)(c1[i - 1 - j] * res[s0 + j]);
                dst[s0 + i] = sat16((int32_t)acc >> 11);
            }
        }
    }
}

/* ---------------------------------------------------------------- mixing */

typedef struct {
    int32_t base[4];
    int16_t ch[4][SUB];      /* left, right, surround, aux */
    int16_t surround[SUB];   /* the previous subframe's surround mix */
    int16_t hist[4];
} Mix;

static void load_state(Mix *m, const uint8_t *st)
{
    memcpy(m->base, st + MX_ST_BASE, sizeof m->base);
    memcpy(m->surround, st + MX_ST_SURROUND, sizeof m->surround);
    memcpy(m->hist, st + MX_ST_HIST, sizeof m->hist);
}

static void save_state(const Mix *m, uint8_t *st)
{
    memcpy(st + MX_ST_BASE, m->base, sizeof m->base);
    memcpy(st + MX_ST_SURROUND, m->surround, sizeof m->surround);
    memcpy(st + MX_ST_HIST, m->hist, sizeof m->hist);
}

static void update_base(Mix *m, const Sfd *s, uint8_t *st)
{
    for (unsigned slot = 0; slot < s->n; slot++) {
        if (!released(s, slot))
            continue;
        int16_t last[4];
        memcpy(last, last_at(st, slot), sizeof last);
        for (int k = 0; k < 4; k++)
            m->base[k] += last[k];
    }
    for (int k = 0; k < 4; k++)
        m->base[k] = (int32_t)(((int64_t)m->base[k] * 0xF850) >> 16);
}

static void init_subframe(Mix *m)
{
    int16_t bs = sat16(m->base[2]), ba = sat16(m->base[3]);
    for (int i = 0; i < SUB; i++) {
        int32_t s = m->surround[i] + bs;
        m->ch[0][i] = sat16(s);
        m->ch[1][i] = sat16(-s);
        m->ch[2][i] = 0;
        m->ch[3][i] = ba;
    }
}

static void render_voice(Mix *m, const Voice *v, int16_t last[4])
{
    int16_t buf[BUF + BUF_PAD];
    int base, offset;
    memset(buf, 0, sizeof buf);

    if (v->frames[0] == 0) {   /* PCM16 */
        int n = (v->count[0] + v->skip[0] + 3) & ~3;
        if (n > BUF)
            n = BUF;
        base = BUF - n;
        cat_s16(buf + base, (size_t)(BUF - base), &v->src[0]);
        if (v->count[1])
            cat_s16(buf, BUF, &v->src[1]);
        offset = v->skip[0];
    } else {                   /* ADPCM */
        int16_t book[128];
        uint8_t raw[RAW_MAX];
        int be = v->book && pc_mem_is_be(v->book);
        for (int i = 0; i < 128; i++)
            book[i] = v->book ? (int16_t)ld16(v->book + 2 * i, be) : 0;
        unsigned f0 = v->frames[0] > BUF / 32 ? BUF / 32 : v->frames[0];
        base = BUF - (int)f0 * 32;
        size_t len = cat_bytes(raw, sizeof raw, &v->src[0]);
        musyx_adpcm_decode(buf + base, raw, len, book, f0, v->skip[0]);
        if (v->frames[1]) {
            unsigned f1 = v->frames[1] > BUF / 32 ? BUF / 32 : v->frames[1];
            len = cat_bytes(raw, sizeof raw, &v->src[1]);
            musyx_adpcm_decode(buf, raw, len, book, f1, v->skip[1]);
        }
        offset = v->skip[0] & 31;
    }

    /* Eight frames at a time, as the microcode's vectors hold them: lane k of a block is frame 8 * block + k. A
     * lane's position (input samples, 16.16) and its four volumes (Q15 in the high half) start at value + k * step
     * and move by 8 * step a block; a position at or past the end goes back to the restart point, checked per lane
     * once a block. Positions are DMEM halfwords there: the buffer is DMEM 0x600-0xA00, halfword 0x300. */
    const int dmem = 0x300;
    int16_t end = (int16_t)(dmem + base + v->end);
    int16_t restart = (int16_t)(v->restart & 0x8000 ? dmem + (v->restart & 0x7FFF) : dmem + base + v->restart);
    uint32_t pos = (uint32_t)(uint16_t)(dmem + base + offset + v->start) << 16 | v->frac;
    uint32_t step = (uint32_t)v->pitch << 4;
    Split p[8], e[4][8];
    for (int k = 0; k < 8; k++) {
        p[k] = split_lane(pos, step, k);
        if (p[k].hi >= end)
            p[k].hi = sat16(sat16(p[k].hi - end) + restart);
        for (int c = 0; c < 4; c++)
            e[c][k] = split_lane((uint32_t)v->env[c], (uint32_t)v->step[c], k);
    }

    for (int i = 0; i < SUB; i += 8) {
        int16_t x[8];
        for (int k = 0; k < 8; k++) {
            int at = p[k].hi - dmem;
            const int16_t *tap = musyx_lut[p[k].lo >> 10];
            if (at >= 0 && at <= BUF + BUF_PAD - 4) {
                x[k] = tap4(buf + at, tap);
            } else {   /* outside the buffer: zeros (the microcode would read other DMEM) */
                int16_t w[4];
                for (int j = 0; j < 4; j++)
                    w[j] = at + j >= 0 && at + j < BUF + BUF_PAD ? buf[at + j] : 0;
                x[k] = tap4(w, tap);
            }
            split_add(&p[k], step << 3);
            if (p[k].hi >= end)
                p[k].hi = sat16(sat16(p[k].hi - end) + restart);
        }
        /* each channel: mix + x * volume (VMULF, VMADH: rounded, saturated once); the last frame's product, saturated,
         * is the slot's last output */
        for (int c = 0; c < 4; c++) {
            for (int k = 0; k < 8; k++) {
                int32_t a = mulf_acc(x[k], e[c][k].hi);
                m->ch[c][i + k] = sat16((int32_t)m->ch[c][i + k] + a);
                if (i + k == SUB - 1)
                    last[c] = sat16(a);
                split_add(&e[c][k], (uint32_t)v->step[c] << 3);
            }
        }
    }
}

/* Returns the output buffer the voice list names; *rendered gets the voices mixed. */
static int16_t *voice_stage(Mix *m, const Sfd *s, uint8_t *st, unsigned *rendered)
{
    *rendered = 0;
    if (first_size(s) == 0)
        return voice_output(s, 0);
    for (unsigned i = 0; i < s->n; i++) {
        Voice v;
        int16_t last[4];
        read_voice(&v, s, (int)i);
        render_voice(m, &v, last);
        memcpy(last_at(st, i), last, sizeof last);
        ++*rendered;
        if (v.output)
            return v.output;
    }
    return NULL;   /* no voice named an output: the microcode would run off the record */
}

static void delay_stage(Mix *m, const Sfd *s)
{
    Delay d;
    if (!read_delay(&d, s->delay))
        return;

    int64_t len = d.length;
    int64_t pos = ((int64_t)s->delay_index * SUB) % len;
    int16_t x[SUB + 3];   /* three samples of history, then the wet subframe */
    int16_t *wet = x + 3;
    memset(wet, 0, SUB * sizeof *wet);
    for (unsigned t = 0; t < d.taps; t++) {
        int64_t at = ((pos - (int64_t)d.tap_delay[t]) % len + len) % len;
        for (int i = 0; i < SUB; i++) {
            wet[i] = sat16(wet[i] + mulf_acc(d.buffer[at], d.tap_gain[t]));   /* VMULF, VMADH */
            if (++at == len)
                at = 0;
        }
    }
    for (int i = 0; i < SUB; i++) {
        m->ch[0][i] = sat16(m->ch[0][i] + wet[i]);
        m->ch[1][i] = sat16(m->ch[1][i] + wet[i]);
    }

    int16_t h[4];
    for (int k = 0; k < 4; k++)
        h[k] = mulf(d.filter[k], d.feedback);
    memcpy(x, m->hist + 1, 3 * sizeof *x);
    memcpy(m->hist, wet + SUB - 4, sizeof m->hist);
    int64_t w = pos;
    for (int i = 0; i < SUB; i++) {
        int32_t f = tap4(x + i, h);
        m->ch[3][i] = sat16(m->ch[3][i] + f);
        d.buffer[w] = m->ch[3][i];
        if (++w == len)
            w = 0;
    }
}

static void interleave(const Mix *m, int16_t *out)
{
    int16_t bl = sat16(m->base[0]), br = sat16(m->base[1]);
    for (int i = 0; i < SUB; i++) {
        out[2 * i] = sat16(m->ch[0][i] + bl);
        out[2 * i + 1] = sat16(m->ch[1][i] + br);
    }
}

/* The mixer's counters (pc_audio_mixer_stats): written by the task's thread, read by the report's. */
static PcMixerStats mixer;

/* The task's thread's CPU time; on Windows the wall clock (its thread times move in scheduler quanta). */
static uint64_t cpu_ns(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (uint64_t)((double)c.QuadPart * 1e9 / (double)f.QuadPart);
#else
    struct timespec t;
#ifdef CLOCK_THREAD_CPUTIME_ID
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t) == 0)
        return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
#endif
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
#endif
}

void musyx_run_task(const void *sfds, size_t count)
{
    static uint8_t scratch_state[MX_STATE_SIZE_N(MX_VOICES_MAX)];   /* if an SFD has no state block */
    const uint8_t *p = sfds;
    unsigned voices = sfd_voices;
    size_t stride = pc_audio_sfd_size(voices);
    Mix m;
    Sfd s;
    uint8_t *st;
    uint64_t t0, used = 0;
    unsigned most = 0;

    if (!p || !count)
        return;
    musyx_lut_init();
    t0 = cpu_ns();

    read_sfd(&s, p, voices);
    st = s.state ? s.state : scratch_state;
    load_state(&m, st);
    for (size_t n = 0; n < count; n++) {
        unsigned rendered;
        read_sfd(&s, p + n * stride, voices);
        st = s.state ? s.state : scratch_state;
        update_base(&m, &s, st);
        init_subframe(&m);
        int16_t *out = voice_stage(&m, &s, st, &rendered);
        delay_stage(&m, &s);
        memcpy(m.surround, m.ch[2], sizeof m.surround);
        if (out) {
            interleave(&m, out);
            pc_audio_note_rendered(out);
        }
        used += rendered;
        if (rendered > most)
            most = rendered;
        if (rendered < sizeof mixer.voices_hist / sizeof *mixer.voices_hist)
            __atomic_fetch_add(&mixer.voices_hist[rendered], 1, __ATOMIC_RELAXED);
        else
            __atomic_fetch_add(&mixer.voices_hist[sizeof mixer.voices_hist / sizeof *mixer.voices_hist - 1], 1,
                               __ATOMIC_RELAXED);
    }
    save_state(&m, st);
    uint64_t took = cpu_ns() - t0;
    __atomic_fetch_add(&mixer.tasks, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&mixer.subframes, count, __ATOMIC_RELAXED);
    __atomic_fetch_add(&mixer.voices, used, __ATOMIC_RELAXED);
    __atomic_fetch_add(&mixer.ns, took, __ATOMIC_RELAXED);
    if (most > __atomic_load_n(&mixer.voices_most, __ATOMIC_RELAXED))
        __atomic_store_n(&mixer.voices_most, most, __ATOMIC_RELAXED);
    if (took / count > __atomic_load_n(&mixer.ns_most, __ATOMIC_RELAXED))
        __atomic_store_n(&mixer.ns_most, took / count, __ATOMIC_RELAXED);
    mixer.slots = voices;
}

void pc_audio_mixer_stats(PcMixerStats *out)
{
    *out = mixer;
    out->tasks = __atomic_load_n(&mixer.tasks, __ATOMIC_RELAXED);
}

/* ---------------------------------------------------------------- task capture (RUSH2049_AUDIO_CAPTURE) */

/* Each task as a self-contained N64-form image, to replay it on another implementation of the microcode
 * (docs/port/AUDIO.md, "Checking against the microcode"). The file is "MXCAP1\0\0", then per task:
 *   u32 'TASK', u32 SFD count, u32 regions, u32 outputs   (little-endian)
 *   regions: u32 address, u32 size, the bytes (big-endian, as in RDRAM), padded to 4
 *   outputs: u32 address, 384 s16 (little-endian): what this interpreter wrote there
 * The SFD array is the first region, at CAP_SFD; every pointer in it is moved to a fresh address of the same
 * alignment mod 8. The state block is CAP_STATE for every task and is not written: the microcode's own state
 * carries over, starting from zero as the game's does.
 * The places below are KSEG0 addresses of the capture's own RDRAM, not the game's (an outside format, which wave 3
 * keeps): offsets from N64_KSEG0 (pc_n64_map.h). */
#define CAP_STATE (N64_KSEG0 + 0x00010000u)       /* the microcode's state block */
#define CAP_SFD (N64_KSEG0 + 0x00100000u)         /* the SFD array, the first region */
#define CAP_OUT (N64_KSEG0 + 0x00180000u)         /* the outputs */
#define CAP_ARENA (N64_KSEG0 + 0x00200000u)       /* the regions the SFD points to, moved here */
#define CAP_ARENA_END (N64_KSEG0 + 0x00400000u)   /* the arena's end, at 4 MiB */

static struct {
    FILE *f;
    int checked;
    uint8_t *buf;          /* the regions being built */
    size_t len, cap;
    uint32_t nregions, next;
    uint32_t out_addr[64];
    int16_t *out_ptr[64];
    uint32_t nouts;
    uint8_t *state;        /* the state block, and its bytes after the last task */
    uint8_t state_after[MX_STATE_SIZE];
} cap;

static void cap_put(const void *p, size_t n)
{
    if (cap.len + n > cap.cap) {
        size_t c = cap.cap ? cap.cap * 2 : 1 << 20;
        while (c < cap.len + n)
            c *= 2;
        uint8_t *b = realloc(cap.buf, c);
        if (!b)
            return;
        cap.buf = b;
        cap.cap = c;
    }
    memcpy(cap.buf + cap.len, p, n);
    cap.len += n;
}

static void cap_u32(uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    cap_put(b, 4);
}

static void be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8), p[1] = (uint8_t)v;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}

/* A region holding size bytes from src at a fresh address with src's alignment mod 8; returns the address of src's
 * first byte there. The region runs from the 8-byte boundary before src to the one after its end, plus 8 bytes, as
 * the RSP's DMA reads whole 8-byte units: inside the game's heap (pc_heap_holds: it holds the sample caches, the
 * codebooks and the outputs) those bytes are copied too, elsewhere they are zero. PCM16 (swap16) is stored big-endian,
 * whatever src's order; a NULL src gives zeros. */
static uint32_t cap_region(const void *src, size_t size, int swap16)
{
    unsigned lead = (unsigned)((uintptr_t)src & 7);
    int in_heap = src && pc_heap_holds(src);
    size_t n = ((lead + size + 7) & ~(size_t)7) + 8;
    uint32_t at = (cap.next + 15) & ~15u;
    if (at + n > CAP_ARENA_END)
        return 0;
    cap.next = at + (uint32_t)n;
    cap_u32(at);
    cap_u32((uint32_t)n);
    size_t start = cap.len;
    uint8_t zero[8] = {0};
    for (size_t i = 0; i < n; i += 8)
        cap_put(zero, 8);
    if (cap.len != start + n)
        return 0;
    if (src) {
        uint8_t *dst = cap.buf + start;
        if (in_heap)
            memcpy(dst, (const uint8_t *)src - lead, n);
        else
            memcpy(dst + lead, src, size);
        if (swap16 && !pc_mem_is_be(src))
            for (size_t i = 0; i + 1 < n; i += 2) {
                uint8_t t = dst[i];
                dst[i] = dst[i + 1];
                dst[i + 1] = t;
            }
    }
    cap.nregions++;
    return at + lead;
}

static void capture_inputs(const void *sfds, size_t count)
{
    if (!cap.checked) {
        const char *path = getenv("RUSH2049_AUDIO_CAPTURE");
        cap.checked = 1;
        if (path && *path) {
            cap.f = fopen(path, "wb");
            if (cap.f)
                fwrite("MXCAP1\0\0", 1, 8, cap.f);
            fprintf(stderr, "audio: %s the tasks to %s\n", cap.f ? "capturing" : "can't capture", path);
        }
    }
    if (!cap.f || !sfds || !count || count > 64)
        return;
    /* SFDs of more than 32 voices (audio.voices) go in the N64 form of that many records (MX_SFD_SIZE_N): the
     * microcode can't replay them, but a summary of the voices can read them (tools/port/capture_summary.py); the
     * SFD array's size over the count gives the records an SFD holds. */
    unsigned nv = sfd_voices;
    const uint8_t *p = sfds;
    size_t stride = pc_audio_sfd_size(nv), isz = MX_SFD_SIZE_N(nv);
    uint8_t *img = calloc(count, isz);
    if (!img)
        return;
    cap.len = 0;
    cap.nregions = 0;
    cap.nouts = 0;
    cap.next = CAP_ARENA;
    for (size_t n = 0; n < count; n++) {
        Sfd s;
        uint8_t *o = img + n * isz;
        read_sfd(&s, p + n * stride, nv);
        be16(o + MX_SFD_COUNT, ((const PcMusyxSfd *)(p + n * stride))->count);
        be16(o + MX_SFD_DELAY_INDEX, s.delay_index);
        be32(o + MX_SFD_RELEASE_MASK, s.mask);
        be32(o + MX_SFD_STATE, CAP_STATE);
        be32(o + MX_SFD_DELAY, 0);   /* the game never sets the delay effect (docs/port/AUDIO.md) */
        if (s.delay)
            fprintf(stderr, "audio: capture: an SFD has a delay effect; not captured\n");
        for (unsigned w = 0; w < MX_MASK_WORDS(nv); w++)
            be32(o + MX_SFD_MASK_AT(nv) + 4 * w, ld32(s.mask_ext + 4 * w, 0));
        int empty = first_size(&s) == 0;
        for (int i = 0; i < (int)nv; i++) {
            Voice v;
            uint8_t *r = o + MX_SFD_VOICES + i * MX_V_SIZE;
            if (empty) {
                v.output = voice_output(&s, 0);
            } else {
                read_voice(&v, &s, i);
                for (int k = 0; k < 4; k++) {
                    be32(r + MX_V_ENV + 4 * k, (uint32_t)v.env[k]);
                    be32(r + MX_V_ENV_STEP + 4 * k, (uint32_t)v.step[k]);
                }
                be16(r + MX_V_PITCH_FRAC, v.frac);
                be16(r + MX_V_PITCH, v.pitch);
                for (int seg = 0; seg < 2; seg++) {
                    /* the loop segment only when the microcode reads it: its fields are stale otherwise */
                    int used = seg == 0 || (v.frames[0] ? v.frames[1] != 0 : v.count[1] != 0);
                    uint8_t *c = r + MX_V_SRC + seg * MX_CAT_SIZE;
                    for (int k = 0; k < 2; k++) {
                        unsigned size = used ? v.src[seg].size[k] : 0;
                        int read = size && (k == 0 || v.src[seg].size[1]);
                        uint32_t at = read && v.src[seg].ptr[k] ? cap_region(v.src[seg].ptr[k], size, !v.frames[0]) : 0;
                        be32(c + (k ? MX_CAT_PTR2 : MX_CAT_PTR1), at);
                        be16(c + (k ? MX_CAT_SIZE2 : MX_CAT_SIZE1), (uint16_t)size);
                    }
                }
                memcpy(r + MX_V_FRAMES, v.frames, 2);
                memcpy(r + MX_V_SKIP, v.skip, 2);
                if (v.frames[0]) {
                    be32(r + MX_V_BOOK, v.book ? cap_region(v.book, 256, 1) : 0);
                } else {
                    be16(r + MX_V_PCM_COUNT, v.count[0]);
                    be16(r + MX_V_PCM_COUNT + 2, v.count[1]);
                }
                be16(r + MX_V_END, v.end);
                be16(r + MX_V_RESTART, v.restart);
                be16(r + MX_V_START, v.start);
            }
            if (v.output) {
                uint32_t at = CAP_OUT + (uint32_t)n * 0x300;
                be32(r + MX_V_OUTPUT, at);
                if (cap.nouts < 64) {
                    cap.out_addr[cap.nouts] = at;
                    cap.out_ptr[cap.nouts++] = v.output;
                }
                break;
            }
            if (empty)
                break;
        }
    }
    /* Flag 1: the game set the state block up again (a new block, or the bytes changed since the last task); the
     * replay then starts both states from zero. Flag 2: and it isn't zero. */
    Sfd s0;
    read_sfd(&s0, p, nv);
    uint32_t flags = 0;
    if (s0.state && (s0.state != cap.state || memcmp(s0.state, cap.state_after, MX_STATE_SIZE))) {
        flags = 1;
        for (int i = 0; i < MX_STATE_SIZE; i++)
            if (s0.state[i])
                flags = 3;
    }
    cap.state = s0.state;
    /* the header and the SFD array go first; the regions built above follow */
    uint8_t h[28];
    uint32_t vals[7] = {0x4B534154u, (uint32_t)count, cap.nregions + 1, cap.nouts, flags, CAP_SFD,
                        (uint32_t)(count * isz)};
    for (int i = 0; i < 7; i++)
        for (int b = 0; b < 4; b++)
            h[4 * i + b] = (uint8_t)(vals[i] >> (8 * b));
    fwrite(h, 1, sizeof h, cap.f);
    fwrite(img, 1, count * isz, cap.f);
    fwrite(cap.buf, 1, cap.len, cap.f);
    free(img);
}

static void capture_outputs(void)
{
    if (!cap.f)
        return;
    if (cap.state)
        memcpy(cap.state_after, cap.state, MX_STATE_SIZE);
    for (uint32_t i = 0; i < cap.nouts; i++) {
        uint8_t b[4];
        for (int k = 0; k < 4; k++)
            b[k] = (uint8_t)(cap.out_addr[i] >> (8 * k));
        fwrite(b, 1, 4, cap.f);
        int16_t le[2 * SUB];
        for (int k = 0; k < 2 * SUB; k++) {
            uint16_t v = (uint16_t)cap.out_ptr[i][k];
            ((uint8_t *)le)[2 * k] = (uint8_t)v;
            ((uint8_t *)le)[2 * k + 1] = (uint8_t)(v >> 8);
        }
        fwrite(le, 1, sizeof le, cap.f);
    }
}

/* ---------------------------------------------------------------- pc_port.h entry points */

void pc_audio_run_task(const void *cmdlist, size_t data_size)
{
    capture_inputs(cmdlist, data_size);
    musyx_run_task(cmdlist, data_size);
    capture_outputs();
}
