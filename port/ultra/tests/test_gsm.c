/* port/ultra/tests/test_gsm.c: the GSM kernels of port/ultra/gsm.c against
 *   - their own MIPS code, run by a small R4300 interpreter from the instruction words in
 *     asm/us/boot_data/kernels.s (GSM_KERNELS_S): every output and every state word must match;
 *   - a GSM 06.10 reference decoder written here from the spec's equations (ref_*). With the kernels' arithmetic
 *     (32-bit, wrapping) it must match always; with the spec's (16-bit, saturating) it must match until the spec
 *     saturates something;
 * kernel by kernel on random and edge inputs, and frame by frame under the game's own decoder (func_80026640 of
 * src/rom/lib_26f60.c, through gsm_game.c). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gsm.h"

int func_80026640(GsmState_26F60 *s, unsigned char *c, short *target);   /* the game's frame decoder */

static int g_fail;
#define CHECK(cond, ...)                                                 \
    do {                                                                 \
        if (!(cond) && g_fail++ < 25) {                                  \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                  \
            printf(__VA_ARGS__);                                         \
            printf("\n");                                                \
        }                                                                \
    } while (0)

/* ---------------------------------------------------------------- inputs */

static uint64_t g_rng = 0x2049C0DE5EED1234ull;
static uint32_t rnd(void)
{
    g_rng ^= g_rng >> 12;
    g_rng ^= g_rng << 25;
    g_rng ^= g_rng >> 27;
    return (uint32_t)((g_rng * 0x2545F4914F6CDD1Dull) >> 32);
}
static int rnd_in(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }
/* A 16-bit value, an edge value one time in four. */
static short rnd_short(void)
{
    static const short edge[] = {-32768, -32767, -16384, -1, 0, 1, 16383, 32766, 32767};
    return rnd() % 4 ? (short)rnd() : edge[rnd() % (sizeof edge / sizeof *edge)];
}
static int32_t rnd_word(void)
{
    static const int32_t edge[] = {INT32_MIN, INT32_MIN + 1, -65536, -32769, -32768, -1, 0, 1, 32767, 32768,
                                   65535, 0x3FFFFFFF, 0x40000000, INT32_MAX};
    switch (rnd() % 4) {
    case 0: return edge[rnd() % (sizeof edge / sizeof *edge)];
    case 1: return (int32_t)rnd();
    default: return rnd_short();
    }
}

/* ---------------------------------------------------------------- the MIPS code, interpreted */

enum {
    CODE_BASE = 0x800277D0, CODE_SIZE = 0x600, ROM_TO_VRAM = 0x7FFFF400,   /* kernels.s: ROM 0x283D0 */
    RPE = 0x800277D0, LTP = 0x800278C4, STP = 0x80027B44, NORM_TABLE = 0x800278BC, QLB_TABLE = 0x80027978,
    RAM_BASE = 0x80100000, RAM_SIZE = 0x4000, RET_MAGIC = 0x80000180,
    E_S = RAM_BASE, E_XMC = RAM_BASE + 0x400, E_ERP = RAM_BASE + 0x500, E_WT = RAM_BASE + 0x600,
    E_OUT = RAM_BASE + 0x800, E_FS = RAM_BASE + 0x1000,   /* E_FS: the frame test's decoder state */
};
static uint8_t g_code[CODE_SIZE], g_ram[RAM_SIZE];
static uint32_t g_reg[32], g_lo, g_hi;
static long g_emu_calls, g_emu_insns;

static uint32_t get16(const uint8_t *p) { return (uint32_t)p[0] << 8 | p[1]; }
static uint32_t get32(const uint8_t *p) { return get16(p) << 16 | get16(p + 2); }
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void put32(uint8_t *p, uint32_t v) { put16(p, v >> 16); put16(p + 2, v); }

/* The kernels' 384 words, from the address comments spimdisasm wrote (the build assembles the same words). */
static int load_kernels(const char *path)
{
    static uint8_t seen[CODE_SIZE / 4];
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("FAIL cannot open %s\n", path);
        return 0;
    }
    char line[512];
    while (fgets(line, sizeof line, f)) {
        unsigned rom, vram, word;
        const char *p = strstr(line, "/* ");
        if (!p || sscanf(p, "/* %x %x %x */", &rom, &vram, &word) != 3 || rom + ROM_TO_VRAM != vram)
            continue;
        if (vram >= CODE_BASE && vram < CODE_BASE + CODE_SIZE && !(vram & 3)) {
            put32(g_code + (vram - CODE_BASE), word);
            seen[(vram - CODE_BASE) / 4] = 1;
        }
    }
    fclose(f);
    for (unsigned i = 0; i < sizeof seen; i++)
        if (!seen[i]) {
            printf("FAIL %s: no word for %08X\n", path, CODE_BASE + 4 * i);
            return 0;
        }
    return 1;
}

static uint8_t *emu_ptr(uint32_t a, unsigned size, int write)
{
    if (a % size)
        return NULL;
    if (a >= RAM_BASE && a - RAM_BASE <= RAM_SIZE - size)
        return g_ram + (a - RAM_BASE);
    if (!write && a >= CODE_BASE && a - CODE_BASE <= CODE_SIZE - size)
        return g_code + (a - CODE_BASE);   /* the inline tables */
    return NULL;
}
static void w16(uint32_t a, int v) { put16(emu_ptr(a, 2, 1), (uint32_t)v); }
static void w32(uint32_t a, int32_t v) { put32(emu_ptr(a, 4, 1), (uint32_t)v); }
static short r16(uint32_t a) { return (short)get16(emu_ptr(a, 2, 0)); }
static int32_t r32(uint32_t a) { return (int32_t)get32(emu_ptr(a, 4, 0)); }

static int emu_fail(const char *what, uint32_t pc, uint32_t x)
{
    CHECK(0, "MIPS: %s at %08X (%08X)", what, pc, x);
    return 0;
}

/* Calls entry with o32 arguments a0-a3 and runs to its return. Only the opcodes the kernels use. Every other
 * register starts as garbage; s0-s7 and sp must come back. */
static int emu_call(uint32_t entry, int32_t a0, int32_t a1, int32_t a2, int32_t a3)
{
    uint32_t *R = g_reg, keep[9];
    for (int i = 1; i < 32; i++)
        R[i] = 0xBAD00000u | (uint32_t)i << 8 | (uint32_t)g_emu_calls;
    R[0] = 0;
    R[4] = (uint32_t)a0, R[5] = (uint32_t)a1, R[6] = (uint32_t)a2, R[7] = (uint32_t)a3;
    R[29] = RAM_BASE + RAM_SIZE - 0x80;
    R[31] = RET_MAGIC;
    memcpy(keep, R + 16, 8 * sizeof *R);
    keep[8] = R[29];
    g_emu_calls++;
    uint32_t pc = entry, npc = entry + 4;
    for (long n = 0; n < 1000000; n++) {
        if (pc == RET_MAGIC) {
            g_emu_insns += n;
            if (memcmp(keep, R + 16, 8 * sizeof *R) || keep[8] != R[29])
                return emu_fail("s0-s7/sp not preserved", entry, 0);
            return 1;
        }
        if (pc - CODE_BASE >= CODE_SIZE || (pc & 3))
            return emu_fail("pc outside the kernels", pc, 0);
        uint32_t in = get32(g_code + (pc - CODE_BASE)), at = pc;
        pc = npc;   /* the delay slot */
        npc += 4;
        uint32_t op = in >> 26, rs = in >> 21 & 31, rt = in >> 16 & 31, rd = in >> 11 & 31, sa = in >> 6 & 31;
        uint32_t uimm = in & 0xFFFF, simm = (uint32_t)(int32_t)(int16_t)uimm, target = at + 4 + (simm << 2);
        uint32_t s = R[rs], t = R[rt], w = 0, size;
        int wreg = 0;
        uint8_t *p;
        switch (op) {
        case 0x00:
            switch (in & 63) {
            case 0x00: wreg = rd, w = t << sa; break;                                   /* sll */
            case 0x03: wreg = rd, w = (uint32_t)((int32_t)t >> sa); break;              /* sra */
            case 0x04: wreg = rd, w = t << (s & 31); break;                             /* sllv */
            case 0x07: wreg = rd, w = (uint32_t)((int32_t)t >> (s & 31)); break;        /* srav */
            case 0x08: npc = s; break;                                                  /* jr */
            case 0x09: wreg = rd, w = at + 8, npc = s; break;                           /* jalr */
            case 0x12: wreg = rd, w = g_lo; break;                                      /* mflo */
            case 0x18: {                                                                /* mult */
                int64_t prod = (int64_t)(int32_t)s * (int32_t)t;
                g_lo = (uint32_t)prod, g_hi = (uint32_t)((uint64_t)prod >> 32);
                break;
            }
            case 0x21: wreg = rd, w = s + t; break;                                     /* addu */
            case 0x23: wreg = rd, w = s - t; break;                                     /* subu */
            case 0x25: wreg = rd, w = s | t; break;                                     /* or */
            default: return emu_fail("unhandled SPECIAL", at, in);
            }
            break;
        case 0x01:
            switch (rt) {
            case 0x00: if ((int32_t)s < 0) npc = target; break;                         /* bltz */
            case 0x01: if ((int32_t)s >= 0) npc = target; break;                        /* bgez */
            case 0x10: wreg = 31, w = at + 8; if ((int32_t)s < 0) npc = target; break;  /* bltzal: links always */
            case 0x11: wreg = 31, w = at + 8; if ((int32_t)s >= 0) npc = target; break; /* bgezal (bal) */
            default: return emu_fail("unhandled REGIMM", at, in);
            }
            break;
        case 0x04: if (s == t) npc = target; break;                                     /* beq */
        case 0x05: if (s != t) npc = target; break;                                     /* bne */
        case 0x06: if ((int32_t)s <= 0) npc = target; break;                            /* blez */
        case 0x07: if ((int32_t)s > 0) npc = target; break;                             /* bgtz */
        case 0x09: wreg = rt, w = s + simm; break;                                      /* addiu */
        case 0x0C: wreg = rt, w = s & uimm; break;                                      /* andi */
        case 0x0D: wreg = rt, w = s | uimm; break;                                      /* ori */
        case 0x20: case 0x21: case 0x23: case 0x25:                                     /* lb lh lw lhu */
            size = op == 0x20 ? 1 : op == 0x23 ? 4 : 2;
            if (!(p = emu_ptr(s + simm, size, 0)))
                return emu_fail("bad load", at, s + simm);
            wreg = rt;
            w = op == 0x20 ? (uint32_t)(int32_t)(int8_t)p[0] : op == 0x21 ? (uint32_t)(int32_t)(int16_t)get16(p)
                : op == 0x25 ? get16(p) : get32(p);
            break;
        case 0x29: case 0x2B:                                                           /* sh sw */
            if (!(p = emu_ptr(s + simm, op == 0x29 ? 2 : 4, 1)))
                return emu_fail("bad store", at, s + simm);
            if (op == 0x29)
                put16(p, t);
            else
                put32(p, t);
            break;
        default: return emu_fail("unhandled opcode", at, in);
        }
        if (wreg)
            R[wreg] = w;
    }
    return emu_fail("no return", entry, 0);
}

/* The native state in the N64's memory: big-endian halfwords, v[] as words. */
static void state_to_emu(const GsmState_26F60 *S, uint32_t a)
{
    const unsigned char *b = (const unsigned char *)S;
    for (unsigned off = 0; off < sizeof *S;) {
        if (off >= GSM_V_OFFSET && off < GSM_V_OFFSET + 32) {
            int32_t x;
            memcpy(&x, b + off, 4);
            w32(a + off, x);
            off += 4;
        } else if (off >= 0x18E) {
            *emu_ptr(a + off, 1, 1) = b[off];
            off++;
        } else {
            short x;
            memcpy(&x, b + off, 2);
            w16(a + off, x);
            off += 2;
        }
    }
}
static void state_from_emu(GsmState_26F60 *S, uint32_t a)
{
    unsigned char *b = (unsigned char *)S;
    for (unsigned off = 0; off < sizeof *S;) {
        if (off >= GSM_V_OFFSET && off < GSM_V_OFFSET + 32) {
            int32_t x = r32(a + off);
            memcpy(b + off, &x, 4);
            off += 4;
        } else if (off >= 0x18E) {
            b[off] = *emu_ptr(a + off, 1, 0);
            off++;
        } else {
            short x = r16(a + off);
            memcpy(b + off, &x, 2);
            off += 2;
        }
    }
}

static int emu_rpe(short xmaxcr, short Mcr, short xMcr[13], short erp[40])
{
    for (int i = 0; i < 13; i++)
        w16(E_XMC + 2 * i, xMcr[i]);
    for (int k = 0; k < 40; k++)
        w16(E_ERP + 2 * k, 0x5A5A);
    int ok = emu_call(RPE, xmaxcr, Mcr, E_XMC, E_ERP);
    for (int i = 0; i < 13; i++)
        xMcr[i] = r16(E_XMC + 2 * i);
    for (int k = 0; k < 40; k++)
        erp[k] = r16(E_ERP + 2 * k);
    return ok;
}
static int emu_ltp(GsmState_26F60 *S, short Ncr, short bcr, const short erp[40])
{
    state_to_emu(S, E_S);
    for (int k = 0; k < 40; k++)
        w16(E_ERP + 2 * k, erp[k]);
    int ok = emu_call(LTP, (int32_t)E_S, Ncr, bcr, E_ERP);
    state_from_emu(S, E_S);
    return ok;
}
/* wt NULL: the state's dp0, as the game passes it. */
static int emu_stp(GsmState_26F60 *S, const short *wt, short s[160])
{
    state_to_emu(S, E_S);
    if (wt)
        for (int k = 0; k < 160; k++)
            w16(E_WT + 2 * k, wt[k]);
    for (int k = 0; k < 160; k++)
        w16(E_OUT + 2 * k, 0x5A5A);
    int ok = emu_call(STP, (int32_t)E_S, (int32_t)(wt ? E_WT : E_S), (int32_t)E_OUT, 0x5A5A);
    state_from_emu(S, E_S);
    for (int k = 0; k < 160; k++)
        s[k] = r16(E_OUT + 2 * k);
    return ok;
}

/* ---------------------------------------------------------------- GSM 06.10 reference, from the spec */

/* g_spec 1: the spec's arithmetic (16-bit; add, sub and mult_r saturate, and g_sat records that one did). 0: the
 * kernels' (products and sums 32-bit and wrapping, 16 bits where they store). */
static int g_spec, g_sat;

static int32_t sat16(int64_t x) { return x > 32767 ? 32767 : x < -32768 ? -32768 : (int32_t)x; }
static int32_t wrap32(int64_t x) { return (int32_t)(uint32_t)(uint64_t)x; }
static int32_t st16(int32_t x) { return (short)x; }
static int32_t op_add(int32_t a, int32_t b)
{
    int64_t x = (int64_t)a + b;
    if (!g_spec)
        return wrap32(x);
    g_sat |= x != sat16(x);
    return sat16(x);
}
static int32_t op_sub(int32_t a, int32_t b)
{
    int64_t x = (int64_t)a - b;
    if (!g_spec)
        return wrap32(x);
    g_sat |= x != sat16(x);
    return sat16(x);
}
static int32_t op_mult_r(int32_t a, int32_t b)
{
    if (!g_spec)
        return wrap32((int64_t)wrap32((int64_t)a * b) + 16384) >> 15;
    if (a == -32768 && b == -32768) {
        g_sat = 1;
        return 32767;
    }
    return (int32_t)(((int64_t)a * b + 16384) >> 15);
}

static const short MIC[8] = {-32, -32, -16, -16, -8, -8, -4, -4};
static const short B[8] = {0, 0, 2048, -2560, 94, -1792, -341, -1144};
static const short INVA[8] = {13107, 13107, 13107, 13107, 19223, 17476, 31454, 29708};
static const short FAC[8] = {18431, 20479, 22527, 24575, 26623, 28671, 30719, 32767};
static const short QLB[4] = {3277, 11469, 21299, 32767};

/* 4.2.8: decoding of the coded log-area ratios. */
static void ref_lar_decode(const short LARc[8], short LARpp[8])
{
    for (int i = 0; i < 8; i++) {
        int32_t temp1 = op_add(LARc[i], MIC[i]) * 1024;
        int32_t temp2 = B[i] * 2;
        temp1 = op_sub(temp1, temp2);
        temp1 = op_mult_r(INVA[i], temp1);
        LARpp[i] = (short)st16(op_add(temp1, temp1));
    }
}

/* 4.2.15: exponent and mantissa of the decoded xmaxc. */
static void ref_xmaxc_norm(int xmaxc, int *exp_out, int *mant_out)
{
    int exp = 0, mant;
    if (xmaxc > 15)
        exp = (xmaxc >> 3) - 1;
    mant = xmaxc - exp * 8;
    if (mant == 0) {
        exp = -4;
        mant = 7;
    } else {
        while (mant <= 7) {
            mant = mant * 2 + 1;
            exp--;
        }
        mant -= 8;
    }
    *exp_out = exp, *mant_out = mant;
}

/* 4.2.16 APCM inverse quantization, 4.2.17 RPE grid positioning. */
static void ref_rpe(int xmaxc, int Mc, const short xMc[13], short xMp[13], short erp[40])
{
    int exp, mant;
    ref_xmaxc_norm(xmaxc, &exp, &mant);
    int32_t temp1 = FAC[mant], temp2 = op_sub(6, exp), temp3 = temp2 > 0 ? 1 << (temp2 - 1) : 0;
    for (int i = 0; i < 13; i++) {
        int32_t temp = (xMc[i] * 2 - 7) * 4096;
        temp = op_mult_r(temp1, temp);
        temp = op_add(temp, temp3);
        xMp[i] = (short)st16(temp >> temp2);
    }
    memset(erp, 0, 40 * sizeof *erp);
    for (int i = 0; i < 13; i++)
        erp[Mc + 3 * i] = xMp[i];
}

/* 4.3.2. hist[0..159] is the spec's drp[-120..39]. */
static void ref_ltp(short hist[160], short *nrp, int Ncr, int bcr, const short erp[40])
{
    short *drp = hist + 120;
    int Nr = Ncr < 40 || Ncr > 120 ? *nrp : Ncr;
    *nrp = (short)Nr;
    int32_t brp = QLB[bcr];
    for (int k = 0; k < 40; k++)
        drp[k] = (short)st16(op_add(erp[k], op_mult_r(brp, drp[k - Nr])));
    for (int k = 0; k < 120; k++)
        drp[-120 + k] = drp[-80 + k];
}

/* 4.2.9.2. Its add saturates in the kernels too (a clamp), so it doesn't count as a spec saturation. */
static int32_t ref_larp_to_rp(int32_t LARp)
{
    int32_t temp = LARp < 0 ? (LARp == -32768 ? 32767 : -LARp) : LARp;
    temp = temp < 11059 ? temp * 2 : temp < 20070 ? temp + 11059 : sat16((temp >> 2) + 26112);
    return LARp < 0 ? -temp : temp;
}

/* 4.2.9.1 interpolation, 4.2.9.2, 4.3.4 short-term synthesis filtering, then 4.3.5-4.3.7 postprocessing.
 * v[0..8], msr: the filter states. */
static void ref_stp(const short prev[8], const short cur[8], int32_t v[9], int32_t *msr, const short wt[160],
                    short out[160])
{
    static const int bound[5] = {0, 13, 27, 40, 160};
    int32_t sr[160];
    for (int part = 0; part < 4; part++) {
        int32_t LARp[8], rrp[8];
        for (int i = 0; i < 8; i++) {
            switch (part) {
            case 0: LARp[i] = op_add(op_add(prev[i] >> 2, cur[i] >> 2), prev[i] >> 1); break;
            case 1: LARp[i] = op_add(prev[i] >> 1, cur[i] >> 1); break;
            case 2: LARp[i] = op_add(op_add(prev[i] >> 2, cur[i] >> 2), cur[i] >> 1); break;
            default: LARp[i] = cur[i]; break;
            }
            rrp[i] = ref_larp_to_rp(LARp[i]);
        }
        for (int k = bound[part]; k < bound[part + 1]; k++) {
            int32_t sri = wt[k];
            for (int i = 1; i <= 8; i++) {
                sri = op_sub(sri, op_mult_r(rrp[8 - i], v[8 - i]));
                v[9 - i] = op_add(v[8 - i], op_mult_r(rrp[8 - i], sri));
            }
            sr[k] = v[0] = sri;
        }
    }
    for (int k = 0; k < 160; k++) {
        *msr = op_add(sr[k], op_mult_r(*msr, 28180));    /* deemphasis */
        int32_t up = sat16(wrap32((int64_t)*msr * 2));   /* upscaling: add(msr, msr); the kernels saturate it too */
        out[k] = (short)st16((up >> 3) * 8);             /* truncation */
    }
    *msr = st16(*msr);
}

typedef struct {
    short hist[160], nrp, LARpp[2][8];
    int32_t v[9], msr;
    int j, spec, sat;
} Ref;

typedef struct {
    short LARc[8], Nc[4], bc[4], Mc[4], xmaxc[4], xMc[52];
} Frame;

static void ref_init(Ref *R, int spec)
{
    memset(R, 0, sizeof *R);
    R->nrp = 40;
    R->spec = spec;
}
static void ref_frame(Ref *R, const Frame *f, short out[160])
{
    short wt[160];
    g_spec = R->spec, g_sat = 0;
    for (int j = 0; j < 4; j++) {
        short erp[40], xMp[13];
        ref_rpe(f->xmaxc[j], f->Mc[j], f->xMc + 13 * j, xMp, erp);
        ref_ltp(R->hist, &R->nrp, f->Nc[j], f->bc[j], erp);
        memcpy(wt + 40 * j, R->hist + 120, 40 * sizeof *wt);
    }
    ref_lar_decode(f->LARc, R->LARpp[R->j]);
    ref_stp(R->LARpp[R->j ^ 1], R->LARpp[R->j], R->v, &R->msr, wt, out);
    R->j ^= 1;
    R->sat |= g_sat;
}

/* ---------------------------------------------------------------- kernel tests */

static int32_t state_v(const GsmState_26F60 *S, int i)
{
    int32_t x;
    memcpy(&x, (const unsigned char *)S + GSM_V_OFFSET + 4 * i, 4);
    return x;
}
static void set_state_v(GsmState_26F60 *S, int i, int32_t x)
{
    memcpy((unsigned char *)S + GSM_V_OFFSET + 4 * i, &x, 4);
}

/* The kernels' history against the spec's drp[-120..39] (hist[0..159]). */
static int same_hist(const GsmState_26F60 *S, const short hist[160])
{
    return !memcmp(S->dp0 + 40, hist, 120 * sizeof *hist) && !memcmp(S->dp0 + 120, hist + 120, 40 * sizeof *hist);
}

static long g_ref_cmp, g_spec_cmp;

static void test_tables(void)
{
    for (int x = 0; x < 8; x++) {
        int exp, mant;
        ref_xmaxc_norm(x, &exp, &mant);
        CHECK((signed char)g_code[NORM_TABLE - CODE_BASE + x] == (signed char)(exp * 16 + mant),
              "D_800278BC[%d] is not xmaxc %d's (exp %d, mant %d)", x, x, exp, mant);
    }
    for (int b = 0; b < 4; b++)
        CHECK((short)get16(g_code + QLB_TABLE - CODE_BASE + 2 * b) == QLB[b], "D_80027978[%d] is not QLB", b);
}

static void check_rpe(short xmaxcr, short Mcr, const short xMc[13], int valid)
{
    short xc[13], xe[13], xr[13], ec[40], ee[40], er[40];
    memcpy(xc, xMc, sizeof xc);
    memcpy(xe, xMc, sizeof xe);
    memset(ec, 0x5A, sizeof ec);
    func_800277D0(xmaxcr, Mcr, xc, ec);
    CHECK(emu_rpe(xmaxcr, Mcr, xe, ee), "RPE MIPS call");
    CHECK(!memcmp(xc, xe, sizeof xc) && !memcmp(ec, ee, sizeof ec), "RPE xmaxc %d Mc %d: C and MIPS differ",
          xmaxcr, Mcr);
    if (!valid)
        return;
    for (g_spec = 0; g_spec < 2; g_spec++) {
        g_sat = 0;
        ref_rpe(xmaxcr, Mcr, xMc, xr, er);
        CHECK(!g_sat, "RPE: the spec saturates on valid input");
        CHECK(!memcmp(xc, xr, sizeof xc) && !memcmp(ec, er, sizeof ec), "RPE xmaxc %d Mc %d: C and %s reference differ",
              xmaxcr, Mcr, g_spec ? "spec" : "kernel-arithmetic");
        g_ref_cmp++, g_spec_cmp += g_spec;
    }
}

static void test_rpe(void)
{
    short x[13];
    for (int xmaxc = 0; xmaxc < 64; xmaxc++)
        for (int Mc = 0; Mc < 4; Mc++)
            for (int n = 0; n < 40; n++) {
                for (int i = 0; i < 13; i++)
                    x[i] = n == 0 ? 0 : n == 1 ? 7 : n == 2 ? (short)(i & 7) : (short)(rnd() & 7);
                check_rpe((short)xmaxc, (short)Mc, x, 1);
            }
    /* Outside the bit fields' ranges: the MIPS code still defines the result. */
    for (int n = 0; n < 3000; n++) {
        for (int i = 0; i < 13; i++)
            x[i] = rnd_short();
        check_rpe(n % 2 ? (short)rnd_in(0, 32767) : (short)rnd_in(0, 63), (short)rnd_in(0, 3), x, 0);
    }
}

static void test_ltp(void)
{
    static const short lags[] = {-32768, -1, 0, 39, 40, 41, 80, 119, 120, 121, 127, 32767};
    for (int n = 0; n < 6000; n++) {
        GsmState_26F60 S, E;
        short erp[40], hist[160], nrp;
        int narrow = n % 2;   /* inputs the spec's 16-bit decoder can take without saturating at once */
        for (int i = 0; i < 180; i++)
            S.dp0[i] = narrow ? (short)rnd_in(-8000, 8000) : rnd_short();
        for (int i = 0; i < 8; i++)
            set_state_v(&S, i, rnd_word());
        for (int i = 0; i < (int)sizeof S - 360; i++)
            ((unsigned char *)S.LARpp)[i] = (unsigned char)rnd();
        S.dp0[GSM_NRP] = (short)rnd_in(40, 120);
        short Ncr = rnd() % 3 ? (short)rnd_in(0, 127) : lags[rnd() % (sizeof lags / sizeof *lags)];
        short bcr = (short)rnd_in(0, 3);
        for (int k = 0; k < 40; k++)
            erp[k] = narrow ? (short)rnd_in(-8000, 8000) : rnd_short();

        /* The spec's drp[-120..39]: drp[-120..-1] is dp0[40..159], and drp[0..39] the newest 40 again. */
        memcpy(hist, S.dp0 + 40, 120 * sizeof *hist);
        memcpy(hist + 120, S.dp0 + 120, 40 * sizeof *hist);
        nrp = S.dp0[GSM_NRP];
        E = S;
        GsmState_26F60 before = S;
        func_800278C4(&S, Ncr, bcr, erp);
        CHECK(emu_ltp(&E, Ncr, bcr, erp), "LTP MIPS call");
        CHECK(!memcmp(&S, &E, sizeof S), "LTP Ncr %d bcr %d: C and MIPS differ", Ncr, bcr);
        CHECK(!memcmp(S.dp0, before.dp0 + 40, 80 * sizeof *S.dp0) && !memcmp(S.dp0 + 160, before.dp0 + 160, 38) &&
              !memcmp(S.LARpp, before.LARpp, sizeof S - 0x168), "LTP: history not shifted, or other state touched");
        for (g_spec = 0; g_spec < 2; g_spec++) {
            short h[160], r = nrp;
            memcpy(h, hist, sizeof h);
            g_sat = 0;
            ref_ltp(h, &r, Ncr, bcr, erp);
            if (g_sat)
                continue;
            CHECK(same_hist(&S, h) && r == S.dp0[GSM_NRP], "LTP Ncr %d bcr %d: C and %s reference differ", Ncr, bcr,
                  g_spec ? "spec" : "kernel-arithmetic");
            g_ref_cmp++, g_spec_cmp += g_spec;
        }
    }
}

/* One short-term synthesis call: C, MIPS, and the references (the spec's when the state is 16-bit). */
static void check_stp(GsmState_26F60 *S, const short *wt_in)
{
    GsmState_26F60 E = *S, in = *S;
    short sc[160], se[160], sr[160], wt[160];
    memcpy(wt, wt_in ? wt_in : S->dp0, sizeof wt);
    func_80027B44(S, wt_in ? wt : S->dp0, sc);
    CHECK(emu_stp(&E, wt_in, se), "STP MIPS call");
    CHECK(!memcmp(S, &E, sizeof E) && !memcmp(sc, se, sizeof sc), "STP j %d: C and MIPS differ", in.j);
    int narrow = 1;
    for (int i = 0; i < 8; i++)
        narrow &= state_v(&in, i) == (short)state_v(&in, i);
    for (g_spec = 0; g_spec < 1 + narrow; g_spec++) {
        int32_t v[9], msr = in.dp0[GSM_MSR];
        for (int i = 0; i < 8; i++)
            v[i] = state_v(&in, i);
        g_sat = 0;
        ref_stp(in.LARpp[in.j == 0], in.LARpp[in.j != 0], v, &msr, wt, sr);
        if (g_sat)
            continue;
        int same = !memcmp(sc, sr, sizeof sc) && msr == S->dp0[GSM_MSR];
        for (int i = 0; i < 8; i++)
            same &= v[i] == state_v(S, i);
        CHECK(same, "STP j %d: C and %s reference differ", in.j, g_spec ? "spec" : "kernel-arithmetic");
        g_ref_cmp++, g_spec_cmp += g_spec;
    }
}

static void test_stp(void)
{
    static const short js[] = {0, 1, 1, 0, 2, -1};
    GsmState_26F60 S;
    short wt[160];
    /* zero in, zero out */
    memset(&S, 0, sizeof S);
    func_80027B44(&S, S.dp0, wt);
    int zero = 1;
    for (int k = 0; k < 160; k++)
        zero &= wt[k] == 0;
    CHECK(zero, "STP: zero input, nonzero output");
    memset(&S, 0, sizeof S);
    check_stp(&S, NULL);
    /* maxima and minima everywhere */
    static const short ext[] = {32767, -32768, -32767, 0};
    for (int a = 0; a < 4; a++)
        for (int b = 0; b < 4; b++)
            for (int c = 0; c < 4; c++) {
                memset(&S, 0, sizeof S);
                for (int i = 0; i < 160; i++)
                    S.dp0[i] = ext[a];
                for (int i = 0; i < 8; i++) {
                    S.LARpp[0][i] = ext[b];
                    S.LARpp[1][i] = ext[c];
                    set_state_v(&S, i, ext[(a + i) & 3]);
                }
                S.dp0[GSM_MSR] = ext[b];
                S.j = (short)(c & 1);
                check_stp(&S, NULL);
            }
    /* random, with the filter state 32-bit or 16-bit and wt in the state (as the game passes it) or apart */
    for (int n = 0; n < 2500; n++) {
        int narrow = n % 2, quiet = n % 4 == 1;
        for (int i = 0; i < 180; i++)
            S.dp0[i] = quiet ? (short)rnd_in(-2000, 2000) : rnd_short();
        for (int i = 0; i < 8; i++) {
            S.LARpp[0][i] = quiet ? (short)rnd_in(-12000, 12000) : rnd_short();
            S.LARpp[1][i] = quiet ? (short)rnd_in(-12000, 12000) : rnd_short();
            set_state_v(&S, i, quiet ? rnd_in(-2000, 2000) : narrow ? rnd_short() : rnd_word());
        }
        S.dp0[GSM_MSR] = quiet ? (short)rnd_in(-2000, 2000) : rnd_short();
        S.j = js[rnd() % (sizeof js / sizeof *js)];
        S.repeat = rnd_short(), S.silence = rnd_short();
        for (int k = 0; k < 160; k++)
            wt[k] = quiet ? (short)rnd_in(-2000, 2000) : rnd_short();
        check_stp(&S, n % 3 ? NULL : wt);
    }
}

/* ---------------------------------------------------------------- frames, through the game's decoder */

enum { KIND_RANDOM, KIND_TAME, KIND_LOUD, KIND_ZERO, KIND_MAX, KIND_MIXED };
static const int lar_bits[8] = {6, 6, 5, 5, 4, 4, 3, 3};

static void make_frame(Frame *f, int kind)
{
    if (kind == KIND_MIXED)
        kind = rnd_in(KIND_RANDOM, KIND_MAX);
    for (int i = 0; i < 8; i++) {
        int top = (1 << lar_bits[i]) - 1, mid = -MIC[i], q = top / 8;
        f->LARc[i] = (short)(kind == KIND_ZERO ? 0 : kind == KIND_MAX ? top
                             : kind == KIND_TAME ? rnd_in(mid - q, mid + q) : rnd_in(0, top));
    }
    for (int j = 0; j < 4; j++) {
        switch (kind) {
        case KIND_ZERO: f->Nc[j] = f->bc[j] = f->Mc[j] = f->xmaxc[j] = 0; break;
        case KIND_MAX: f->Nc[j] = 127, f->bc[j] = 3, f->Mc[j] = 3, f->xmaxc[j] = 63; break;
        case KIND_TAME:
            f->Nc[j] = (short)rnd_in(40, 120), f->bc[j] = (short)rnd_in(0, 1), f->Mc[j] = (short)rnd_in(0, 3);
            f->xmaxc[j] = (short)rnd_in(0, 30);
            break;
        case KIND_LOUD:
            f->Nc[j] = 40, f->bc[j] = 3, f->Mc[j] = (short)rnd_in(0, 3), f->xmaxc[j] = (short)rnd_in(56, 63);
            break;
        default:
            f->Nc[j] = (short)rnd_in(0, 127), f->bc[j] = (short)rnd_in(0, 3), f->Mc[j] = (short)rnd_in(0, 3);
            f->xmaxc[j] = (short)rnd_in(0, 63);
            break;
        }
        for (int i = 0; i < 13; i++)
            f->xMc[13 * j + i] = (short)(kind == KIND_ZERO ? 0 : kind == KIND_MAX ? 7
                                         : kind == KIND_LOUD ? (int)(rnd() & 1) * 7 : rnd_in(0, 7));
    }
}

/* The 33-byte frame lib_26f60.c's func_80026640 reads: a 0xD nibble, then the fields MSB first. */
static void pack_frame(const Frame *f, unsigned char c[33])
{
    int pos = 0;
    memset(c, 0, 33);
#define PUT(val, n)                                                       \
    for (int b_ = (n) - 1; b_ >= 0; b_--, pos++)                          \
        if ((val) >> b_ & 1)                                              \
            c[pos >> 3] |= (unsigned char)(0x80 >> (pos & 7));
    PUT(0xD, 4);
    for (int i = 0; i < 8; i++)
        PUT(f->LARc[i], lar_bits[i]);
    for (int j = 0; j < 4; j++) {
        PUT(f->Nc[j], 7);
        PUT(f->bc[j], 2);
        PUT(f->Mc[j], 2);
        PUT(f->xmaxc[j], 6);
        for (int i = 0; i < 13; i++)
            PUT(f->xMc[13 * j + i], 3);
    }
#undef PUT
}

/* func_80026558 and func_80026360 over the MIPS kernels, the state at E_FS in the N64's memory. */
static int emu_frame(const Frame *f, short out[160])
{
    int ok = 1;
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 13; i++)
            w16(E_XMC + 2 * i, f->xMc[13 * j + i]);
        ok &= emu_call(RPE, f->xmaxc[j], f->Mc[j], E_XMC, E_ERP);
        ok &= emu_call(LTP, (int32_t)E_FS, f->Nc[j], f->bc[j], E_ERP);
    }
    short LARpp[8], j = r16(E_FS + 0x188);
    g_spec = 0;
    ref_lar_decode(f->LARc, LARpp);   /* the game's C formula, for every coded value */
    for (int i = 0; i < 8; i++)
        w16(E_FS + 0x168 + 16 * j + 2 * i, LARpp[i]);
    ok &= emu_call(STP, (int32_t)E_FS, (int32_t)E_FS, (int32_t)E_OUT, 0);
    w16(E_FS + 0x188, j ^ 1);
    for (int k = 0; k < 160; k++)
        out[k] = r16(E_OUT + 2 * k);
    return ok;
}

static void test_frames(void)
{
    static const struct { int kind, streams, frames; } plan[] = {
        {KIND_TAME, 60, 50}, {KIND_RANDOM, 40, 40}, {KIND_MIXED, 30, 40}, {KIND_LOUD, 10, 30},
        {KIND_ZERO, 1, 30}, {KIND_MAX, 1, 30},
    };
    static const char *names[] = {"random", "tame", "loud", "zero", "max", "mixed"};
    for (unsigned p = 0; p < sizeof plan / sizeof *plan; p++) {
        long frames = 0, spec_frames = 0, spec_diverged = 0;
        const char *name = names[plan[p].kind];
        for (int st = 0; st < plan[p].streams; st++) {
            GsmState_26F60 A, E;
            Ref W, D;
            memset(&A, 0, sizeof A);
            A.dp0[GSM_NRP] = 40;   /* func_80025EB0: bzero, then block_count (nrp) = 40 */
            state_to_emu(&A, E_FS);
            ref_init(&W, 0);
            ref_init(&D, 1);
            for (int n = 0; n < plan[p].frames; n++) {
                Frame f;
                unsigned char c[33];
                short oa[160], oe[160], ow[160], od[160];
                make_frame(&f, plan[p].kind);
                pack_frame(&f, c);
                func_80026640(&A, c, oa);
                CHECK(emu_frame(&f, oe), "frame MIPS calls");
                state_from_emu(&E, E_FS);
                ref_frame(&W, &f, ow);
                int was_clean = !D.sat;
                ref_frame(&D, &f, od);
                frames++;
                CHECK(!memcmp(oa, oe, sizeof oa) && !memcmp(&A, &E, sizeof A),
                      "%s stream %d frame %d: the game's decoder and the MIPS kernels differ", name, st, n);
                int same = !memcmp(oa, ow, sizeof oa) && same_hist(&A, W.hist) && A.dp0[GSM_NRP] == W.nrp &&
                           A.dp0[GSM_MSR] == W.msr && A.j == W.j;
                for (int i = 0; i < 8; i++)
                    same &= state_v(&A, i) == W.v[i];
                CHECK(same, "%s stream %d frame %d: the game's decoder and the kernel-arithmetic reference differ",
                      name, st, n);
                if (!D.sat) {
                    CHECK(!memcmp(oa, od, sizeof oa), "%s stream %d frame %d: the game's decoder and the spec differ",
                          name, st, n);
                    spec_frames++;
                } else if (was_clean && memcmp(oa, od, sizeof oa)) {
                    spec_diverged++;
                }
            }
        }
        printf("  %-6s %4ld frames: %4ld match the spec decoder before it saturates; %ld stream(s) diverge there\n",
               name, frames, spec_frames, spec_diverged);
        if (plan[p].kind == KIND_TAME)
            CHECK(spec_frames * 10 >= frames * 9, "too few tame frames compared with the spec (%ld of %ld)",
                  spec_frames, frames);
    }
}

int main(void)
{
    if (!load_kernels(GSM_KERNELS_S))
        return 1;
    test_tables();
    test_rpe();
    test_ltp();
    test_stp();
    printf("gsm: kernels: %ld MIPS calls (%ld instructions) and %ld reference checks (%ld with the spec's "
           "arithmetic) agree\n", g_emu_calls, g_emu_insns, g_ref_cmp, g_spec_cmp);
    test_frames();
    if (g_fail) {
        printf("gsm: %d failure(s)\n", g_fail);
        return 1;
    }
    printf("gsm: OK\n");
    return 0;
}
