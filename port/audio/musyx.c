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
 * and store the state back. Arithmetic follows the RSP's style (Q15 products, arithmetic shifts, saturation
 * to s16); rounding inside the microcode's vector ops is not reproduced (docs/port/AUDIO.md). */
#include <string.h>

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

static inline void *n64ptr(uint32_t addr)
{
    return addr ? pc_n64_ptr(addr) : NULL;
}

/* A native-form pointer field: a host pointer, or below 4 GB an N64 address (the RDRAM window at host
 * 0x80000000, K0, K1 or physical). Host data is never below 4 GB on a 64-bit host: the executable is PIE. */
static inline void *resolve(const void *p)
{
#if UINTPTR_MAX > 0xFFFFFFFFu
    uintptr_t v = (uintptr_t)p;
    if (v < ((uintptr_t)1 << 32))
        return n64ptr((uint32_t)v);
#endif
    return (void *)(uintptr_t)p;
}

/* ---------------------------------------------------------------- records, decoded from either form */

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
    const uint8_t *delay;   /* N64 form: raw bytes; native form: a PcMusyxDelay */
    const uint8_t *voices;
    int be;                 /* N64 form, big-endian */
} Sfd;

typedef struct {
    int16_t *buffer;
    uint32_t length;
    unsigned taps;
    int16_t feedback;
    uint32_t tap_delay[8];
    int16_t tap_gain[8];
    int16_t filter[4];
} Delay;

static void read_sfd(Sfd *s, const uint8_t *p, int form)
{
    if (form == PC_AUDIO_FORM_N64) {
        int be = pc_mem_is_be(p);
        s->be = be;
        s->delay_index = ld16(p + MX_SFD_DELAY_INDEX, be);
        s->mask = ld32(p + MX_SFD_RELEASE_MASK, be);
        s->state = n64ptr(ld32(p + MX_SFD_STATE, be));
        s->delay = n64ptr(ld32(p + MX_SFD_DELAY, be));
        s->voices = p + MX_SFD_VOICES;
    } else {
        const PcMusyxSfd *n = (const PcMusyxSfd *)p;
        s->be = 0;
        s->delay_index = n->delay_index;
        s->mask = n->release_mask;
        s->state = resolve(n->state);
        s->delay = resolve(n->delay);
        s->voices = (const uint8_t *)n->voices;
    }
}

static const uint8_t *voice_at(const Sfd *s, int i, int form)
{
    return s->voices + (size_t)i * (form == PC_AUDIO_FORM_N64 ? MX_V_SIZE : sizeof(PcMusyxVoice));
}

/* Bytes in voice 0's main range: zero means the SFD has no voices. */
static unsigned first_size(const Sfd *s, int form)
{
    const uint8_t *p = voice_at(s, 0, form);
    if (form == PC_AUDIO_FORM_N64)
        return ld16(p + MX_V_SRC + MX_CAT_SIZE1, s->be);
    return (uint16_t)((const PcMusyxVoice *)p)->src[0].size1;
}

static int16_t *voice_output(const Sfd *s, int i, int form)
{
    const uint8_t *p = voice_at(s, i, form);
    if (form == PC_AUDIO_FORM_N64)
        return n64ptr(ld32(p + MX_V_OUTPUT, s->be));
    return resolve(((const PcMusyxVoice *)p)->output);
}

static void read_voice(Voice *v, const Sfd *s, int i, int form)
{
    const uint8_t *p = voice_at(s, i, form);
    if (form == PC_AUDIO_FORM_N64) {
        int be = s->be;
        for (int k = 0; k < 4; k++) {
            v->env[k] = (int32_t)ld32(p + MX_V_ENV + 4 * k, be);
            v->step[k] = (int32_t)ld32(p + MX_V_ENV_STEP + 4 * k, be);
        }
        v->frac = ld16(p + MX_V_PITCH_FRAC, be);
        v->pitch = ld16(p + MX_V_PITCH, be);
        for (int seg = 0; seg < 2; seg++) {
            const uint8_t *c = p + MX_V_SRC + seg * MX_CAT_SIZE;
            v->src[seg].ptr[0] = n64ptr(ld32(c + MX_CAT_PTR1, be));
            v->src[seg].ptr[1] = n64ptr(ld32(c + MX_CAT_PTR2, be));
            v->src[seg].size[0] = ld16(c + MX_CAT_SIZE1, be);
            v->src[seg].size[1] = ld16(c + MX_CAT_SIZE2, be);
        }
        memcpy(v->frames, p + MX_V_FRAMES, 2);
        memcpy(v->skip, p + MX_V_SKIP, 2);
        v->book = v->frames[0] ? n64ptr(ld32(p + MX_V_BOOK, be)) : NULL;
        v->count[0] = ld16(p + MX_V_PCM_COUNT, be);
        v->count[1] = ld16(p + MX_V_PCM_COUNT + 2, be);
        v->output = n64ptr(ld32(p + MX_V_OUTPUT, be));
        v->end = ld16(p + MX_V_END, be);
        v->restart = ld16(p + MX_V_RESTART, be);
        v->start = ld16(p + MX_V_START, be);
    } else {
        const PcMusyxVoice *n = (const PcMusyxVoice *)p;
        memcpy(v->env, n->env, sizeof v->env);
        memcpy(v->step, n->env_step, sizeof v->step);
        v->frac = n->pitch_frac;
        v->pitch = n->pitch;
        for (int seg = 0; seg < 2; seg++) {
            v->src[seg].ptr[0] = resolve(n->src[seg].ptr1);
            v->src[seg].ptr[1] = resolve(n->src[seg].ptr2);
            v->src[seg].size[0] = (uint16_t)n->src[seg].size1;
            v->src[seg].size[1] = (uint16_t)n->src[seg].size2;
        }
        memcpy(v->frames, n->frames, 2);
        memcpy(v->skip, n->skip, 2);
        v->book = v->frames[0] ? resolve(n->u.book) : NULL;
        v->count[0] = (uint16_t)n->u.pcm.count[0];
        v->count[1] = (uint16_t)n->u.pcm.count[1];
        v->output = resolve(n->output);
        v->end = n->end;
        v->restart = n->restart;
        v->start = n->start;
    }
}

static int read_delay(Delay *d, const uint8_t *p, int form)
{
    if (!p)
        return 0;
    if (form == PC_AUDIO_FORM_N64) {
        int be = pc_mem_is_be(p);
        d->buffer = n64ptr(ld32(p + MX_D_BUFFER, be));
        d->length = ld32(p + MX_D_LENGTH, be);
        d->taps = ld16(p + MX_D_TAPS, be);
        d->feedback = (int16_t)ld16(p + MX_D_FEEDBACK, be);
        for (int i = 0; i < 8; i++) {
            d->tap_delay[i] = ld32(p + MX_D_TAP_DELAY + 4 * i, be);
            d->tap_gain[i] = (int16_t)ld16(p + MX_D_TAP_GAIN + 2 * i, be);
        }
        for (int i = 0; i < 4; i++)
            d->filter[i] = (int16_t)ld16(p + MX_D_FILTER + 2 * i, be);
    } else {
        const PcMusyxDelay *n = (const PcMusyxDelay *)p;
        d->buffer = resolve(n->buffer);
        d->length = n->length;
        d->taps = n->taps;
        d->feedback = n->feedback;
        memcpy(d->tap_delay, n->tap_delay, sizeof d->tap_delay);
        memcpy(d->tap_gain, n->tap_gain, sizeof d->tap_gain);
        memcpy(d->filter, n->filter, sizeof d->filter);
    }
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
            int64_t p2 = dst[s0 - 2], p1 = dst[s0 - 1];
            for (int i = 0; i < group_len[grp]; i++) {
                int64_t acc = ((int64_t)res[s0 + i] << 11) + c0[i] * p2 + c1[i] * p1;
                for (int j = 0; j < i; j++)
                    acc += (int64_t)c1[i - 1 - j] * res[s0 + j];
                dst[s0 + i] = sat16(acc >> 11);
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

static void update_base(Mix *m, uint32_t mask, const uint8_t *st)
{
    for (int slot = 0; slot < MX_VOICES; slot++) {
        if (!(mask >> slot & 1))
            continue;
        int16_t last[4];
        memcpy(last, st + MX_ST_LAST + slot * 8, sizeof last);
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

    int pos = base + offset + v->start;
    int end = base + v->end;
    int restart = (v->restart & 0x7FFF) + (v->restart & 0x8000 ? 0 : base);
    uint32_t acc = v->frac;
    uint32_t step = (uint32_t)v->pitch << 4;
    uint32_t env[4];
    for (int k = 0; k < 4; k++)
        env[k] = (uint32_t)v->env[k];

    for (int i = 0; i < SUB; i++) {
        const int16_t *tap = musyx_lut[(acc >> 10) & 63];
        pos += (int)(acc >> 16);
        acc = (acc & 0xFFFF) + step;
        if (pos >= end)
            pos = restart + (pos - end);

        int64_t s = 0;
        if (pos >= 0 && pos <= BUF + BUF_PAD - 4) {
            for (int j = 0; j < 4; j++)
                s += (int32_t)buf[pos + j] * tap[j];
        } else {
            for (int j = 0; j < 4; j++)
                if (pos + j >= 0 && pos + j < BUF + BUF_PAD)
                    s += (int32_t)buf[pos + j] * tap[j];
        }
        int32_t x = sat16(s >> 15);

        for (int k = 0; k < 4; k++) {
            int32_t a = (x * ((int32_t)env[k] >> 16)) >> 15;
            last[k] = sat16(a);
            m->ch[k][i] = sat16((int32_t)m->ch[k][i] + a);
            env[k] += (uint32_t)v->step[k];
        }
    }
}

/* Returns the output buffer the voice list names. */
static int16_t *voice_stage(Mix *m, const Sfd *s, int form, uint8_t *st)
{
    if (first_size(s, form) == 0)
        return voice_output(s, 0, form);
    for (int i = 0; i < MX_VOICES; i++) {
        Voice v;
        int16_t last[4];
        read_voice(&v, s, i, form);
        render_voice(m, &v, last);
        memcpy(st + MX_ST_LAST + i * 8, last, sizeof last);
        if (v.output)
            return v.output;
    }
    return NULL;   /* no voice named an output: the microcode would run off the record */
}

static void delay_stage(Mix *m, const Sfd *s, int form)
{
    Delay d;
    if (!read_delay(&d, s->delay, form))
        return;

    int64_t len = d.length;
    int64_t pos = ((int64_t)s->delay_index * SUB) % len;
    int16_t x[SUB + 3];   /* three samples of history, then the wet subframe */
    int16_t *wet = x + 3;
    memset(wet, 0, SUB * sizeof *wet);
    for (unsigned t = 0; t < d.taps; t++) {
        int64_t at = ((pos - (int64_t)d.tap_delay[t]) % len + len) % len;
        for (int i = 0; i < SUB; i++) {
            int16_t v = d.buffer[at];
            wet[i] = sat16(wet[i] + ((v * d.tap_gain[t]) >> 15));
            if (++at == len)
                at = 0;
        }
    }
    for (int i = 0; i < SUB; i++) {
        m->ch[0][i] = sat16(m->ch[0][i] + wet[i]);
        m->ch[1][i] = sat16(m->ch[1][i] + wet[i]);
    }

    int32_t h[4];
    for (int k = 0; k < 4; k++)
        h[k] = (d.feedback * d.filter[k]) >> 15;
    memcpy(x, m->hist + 1, 3 * sizeof *x);
    memcpy(m->hist, wet + SUB - 4, sizeof m->hist);
    int64_t w = pos;
    for (int i = 0; i < SUB; i++) {
        int64_t f = ((int64_t)h[0] * x[i] + (int64_t)h[1] * x[i + 1] + (int64_t)h[2] * x[i + 2] +
                     (int64_t)h[3] * x[i + 3]) >> 15;
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

void musyx_run_task(const void *sfds, size_t count, int form)
{
    static uint8_t scratch_state[MX_STATE_SIZE];   /* if an SFD has no state block */
    const uint8_t *p = sfds;
    size_t stride = form == PC_AUDIO_FORM_N64 ? MX_SFD_SIZE : sizeof(PcMusyxSfd);
    Mix m;
    Sfd s;
    uint8_t *st;

    if (!p || !count)
        return;
    musyx_lut_init();

    read_sfd(&s, p, form);
    st = s.state ? s.state : scratch_state;
    load_state(&m, st);
    for (size_t n = 0; n < count; n++) {
        read_sfd(&s, p + n * stride, form);
        st = s.state ? s.state : scratch_state;
        update_base(&m, s.mask, st);
        init_subframe(&m);
        int16_t *out = voice_stage(&m, &s, form, st);
        delay_stage(&m, &s, form);
        memcpy(m.surround, m.ch[2], sizeof m.surround);
        if (out)
            interleave(&m, out);
    }
    save_state(&m, st);
}

/* ---------------------------------------------------------------- pc_port.h entry points */

#ifndef PC_AUDIO_DEFAULT_FORM
#define PC_AUDIO_DEFAULT_FORM PC_AUDIO_FORM_NATIVE
#endif

static int task_form = PC_AUDIO_DEFAULT_FORM;

void pc_audio_set_task_form(int form)
{
    task_form = form == PC_AUDIO_FORM_N64 ? PC_AUDIO_FORM_N64 : PC_AUDIO_FORM_NATIVE;
}

int pc_audio_task_form(void)
{
    return task_form;
}

void pc_audio_run_task_form(const void *sfds, size_t count, int form)
{
    musyx_run_task(sfds, count, form);
}

void pc_audio_run_task(const void *cmdlist, size_t data_size)
{
    musyx_run_task(cmdlist, data_size, task_form);
}
