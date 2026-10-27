/* port/assets/tests/wad_build.h: a synthetic big-endian WAD for the WAD tests (test_wad.c, wad_game.c), laid out
 * the way the game's code reads one (docs/port/ASSETS.md §3.1, §3.2). No ROM data: every value is made up.
 *
 *   0x000 header: TOC offset, entry count
 *   0x008 OBHD   2 models; model 0 has 2 LODs, model 1 has 1
 *   0x0B8 TXHD   3 textures, sorted by name (MBOX_FindTexture bsearches them)
 *   0x124 PLHD   2 palettes
 *   0x154 PTHD   3 target paths: 0 and 1 share their middle point, 2 has none
 *   0x1C0 PATH   4 points of 0x44
 *   0x2D0 TXLD   SETTIMG, SETTILE, E1+04 (a branch), SETTILESIZE, ENDDL; texture 1's list
 *   0x300 OBJS   display lists (LOD 0: VTX, TRI1, a LOD marker the load flags keep, a call, a LOD marker they
 *                turn into a branch, ENDDL; the called list; the branch target; LOD 1 and model 1's lists) and
 *                three Vtx
 *   0x400 IMAG   texels and a TLUT
 *   0x480 TOC    OBHD, TXHD, PLHD, PTHD, TXLD, IMAG, and an OBJS entry the game never asks for */
#ifndef WAD_BUILD_H
#define WAD_BUILD_H

#include <stdint.h>
#include <string.h>

#include "support.h"

enum {
    W_OBHD = 0x008, W_TXHD = 0x0B8, W_PLHD = 0x124, W_PTHD = 0x154, W_PATH = 0x1C0, W_TXLD = 0x2D0,
    W_OBJS = 0x300, W_IMAG = 0x400, W_TOC = 0x480, W_TOC_N = 7, W_SIZE = W_TOC + W_TOC_N * 12,
    /* inside OBJS */
    W_DL0 = 0x300,          /* LOD 0 of model 0, 6 commands */
    W_DL_SUB = 0x330,       /* called from DL0: TRI2, ENDDL */
    W_DL_TAIL = 0x340,      /* the branch DL0's second LOD marker becomes: TRI1, ENDDL */
    W_DL1 = 0x350,          /* LOD 1 of model 0: TRI1, ENDDL */
    W_DL_M1 = 0x360,        /* model 1: ENDDL */
    W_VTX = 0x370,          /* 3 Vtx */
    W_TLUT = 0x440,
};

#define W_TRI1_A 0x05000204u   /* TRI1 (0, 1, 2) * 2 */
#define W_TRI1_B 0x05020406u
#define W_TRI2_0 0x06000204u
#define W_TRI2_1 0x00020406u

static void wad_name(uint8_t *p, const char *name)
{
    memset(p, 0, 16);
    memcpy(p, name, strlen(name));
}

static void wad_f32(uint8_t *p, float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    test_put32(p, u);
}

static void wad_cmd(uint8_t *p, uint32_t w0, uint32_t w1)
{
    test_put32(p, w0);
    test_put32(p + 4, w1);
}

/* Writes the WAD into buf (at least W_SIZE bytes, zeroed by the caller). Returns its size. */
static size_t wad_build(uint8_t *buf)
{
    static const struct { uint32_t tag, off, n; } toc[W_TOC_N] = {
        {0x4F424844, W_OBHD, 2}, {0x54584844, W_TXHD, 3}, {0x504C4844, W_PLHD, 2}, {0x50544844, W_PTHD, 3},
        {0x54584C44, W_TXLD, 0x30}, {0x494D4147, W_IMAG, 0x80}, {0x4F424A53, W_OBJS, 0x100},
    };
    test_put32(buf, W_TOC);
    test_put32(buf + 4, W_TOC_N);
    for (int i = 0; i < W_TOC_N; i++) {
        test_put32(buf + W_TOC + 12 * i, toc[i].tag);
        test_put32(buf + W_TOC + 12 * i + 4, toc[i].off);
        test_put32(buf + W_TOC + 12 * i + 8, toc[i].n);
    }

    /* OBHD */
    for (int m = 0; m < 2; m++) {
        uint8_t *r = buf + W_OBHD + 0x58 * m;
        wad_name(r, m ? "model1" : "model0");
        wad_f32(r + 0x10, m ? 2.5f : 100.0f);
        test_put16(r + 0x14, 0x1234);
        test_put16(r + 0x16, m ? 1 : 2);
        for (int k = 0; k < (m ? 1 : 2); k++) {
            uint8_t *lod = r + 0x18 + 0x10 * k;
            test_put16(lod, (uint16_t)(k + 1));                  /* texidx */
            test_put16(lod + 2, 0x0300);                         /* flags */
            wad_f32(lod + 4, k ? 500.0f : 50.0f);                /* lodRad */
            test_put32(lod + 8, m ? W_DL_M1 : k ? W_DL1 : W_DL0);  /* displayList, file-relative */
            test_put32(lod + 12, W_VTX);                         /* vtxList */
        }
    }

    /* TXHD: a texel texture, a TXLD-list texture, a CI texture with a palette */
    static const char *tex_names[3] = {"alpha", "bravo", "charlie"};
    for (int t = 0; t < 3; t++) {
        uint8_t *r = buf + W_TXHD + 0x24 * t;
        wad_name(r, tex_names[t]);
        test_put16(r + 0x10, (uint16_t)(32 << t));     /* width */
        test_put16(r + 0x12, (uint16_t)(16 + t));      /* height */
        r[0x14] = (uint8_t)(t + 1);                    /* size */
        r[0x15] = (uint8_t)(t * 2);                    /* format */
        test_put16(r + 0x16, t == 2 ? 1 : 0xFFFF);     /* palIndex */
        test_put32(r + 0x18, t == 1 ? (uint32_t)(W_TXLD - W_IMAG) : (uint32_t)(0x10 * t));   /* texBase, IMAG-relative */
        test_put32(r + 0x1C, t == 1 ? 0x00030000u : 0x08010000u);   /* flags: 0x08000000 = texels */
        test_put32(r + 0x20, 0xA0B0C0D0u + t);         /* xflags */
    }

    /* PLHD */
    for (int p = 0; p < 2; p++) {
        uint8_t *r = buf + W_PLHD + 0x18 * p;
        wad_name(r, p ? "pal1" : "pal0");
        r[0x10] = (uint8_t)(p * 16);
        r[0x11] = (uint8_t)(p * 16 + 15);
        test_put16(r + 0x12, 0x0102);
        test_put32(r + 0x14, W_TLUT - W_IMAG + 0x20 * p);   /* palBase: IMAG-relative */
    }

    /* PTHD and PATH: path 0 = points 0..2, path 1 = points 2..3, path 2 = none */
    static const int first[3] = {0, 2, 0}, num[3] = {3, 2, 0};
    for (int p = 0; p < 3; p++) {
        uint8_t *r = buf + W_PTHD + 0x24 * p;
        wad_name(r, p == 0 ? "path0" : p == 1 ? "path1" : "path2");
        test_put32(r + 0x10, 0x00010000u << p);
        test_put16(r + 0x14, (uint16_t)num[p]);
        test_put16(r + 0x16, (uint16_t)(p - 1));
        test_put32(r + 0x18, W_PATH + 0x44 * first[p] + 0x40);   /* trigPtr: some file offset */
        test_put32(r + 0x1C, W_PATH + 0x44 * first[p]);          /* pts */
        test_put32(r + 0x20, 0);                                 /* parent */
    }
    for (int i = 0; i < 4; i++) {
        uint8_t *pt = buf + W_PATH + 0x44 * i;
        for (int w = 0; w < 16; w++)
            wad_f32(pt + 4 * w, (float)(i * 100 + w));
        test_put32(pt + 0x40, 0xF1A90000u + (uint32_t)i);     /* flags */
    }

    /* TXLD: SETTIMG (IMAG-relative), SETTILE, E1 + 04 (IMAG-relative), SETTILESIZE, ENDDL */
    wad_cmd(buf + W_TXLD, 0xFD100000u, 0x00000010u);
    wad_cmd(buf + W_TXLD + 8, 0xF5100000u, 0x07000000u);
    wad_cmd(buf + W_TXLD + 16, 0xE1000000u, 0x00000020u);
    wad_cmd(buf + W_TXLD + 24, 0x04000000u, 0x00000000u);
    wad_cmd(buf + W_TXLD + 32, 0xF2000000u, 0x0007C03Cu);
    wad_cmd(buf + W_TXLD + 40, 0xDF000000u, 0);

    /* OBJS */
    wad_cmd(buf + W_DL0, 0x01003006u, W_VTX);          /* VTX 3 */
    wad_cmd(buf + W_DL0 + 8, W_TRI1_A, 0);
    wad_cmd(buf + W_DL0 + 16, 0xE0010001u, W_DL1);     /* LOD marker, bit 1: kept with mask 2 | 8 */
    wad_cmd(buf + W_DL0 + 24, 0xDE000000u, W_DL_SUB);  /* call */
    wad_cmd(buf + W_DL0 + 32, 0xE0010002u, W_DL_TAIL); /* LOD marker, bit 2: becomes a branch with mask 2 | 8 */
    wad_cmd(buf + W_DL0 + 40, 0xDF000000u, 0);
    wad_cmd(buf + W_DL_SUB, W_TRI2_0, W_TRI2_1);
    wad_cmd(buf + W_DL_SUB + 8, 0xDF000000u, 0);
    wad_cmd(buf + W_DL_TAIL, W_TRI1_B, 0);
    wad_cmd(buf + W_DL_TAIL + 8, 0xDF000000u, 0);
    wad_cmd(buf + W_DL1, W_TRI1_A, 0);
    wad_cmd(buf + W_DL1 + 8, 0xDF000000u, 0);
    wad_cmd(buf + W_DL_M1, 0xDF000000u, 0);
    for (int v = 0; v < 3; v++) {
        uint8_t *p = buf + W_VTX + 16 * v;
        test_put16(p, (uint16_t)(v * 10));
        test_put16(p + 2, (uint16_t)(v * 20));
        test_put16(p + 4, (uint16_t)(v * 30));
        test_put16(p + 6, (uint16_t)((v < 2 ? 0x8000 : 0) | 32));   /* next follows; texture width 32 */
        test_put16(p + 8, (uint16_t)(v * 256));                     /* s */
        test_put16(p + 10, (uint16_t)(v * 128));                    /* t */
        p[12] = 0x11, p[13] = 0x22, p[14] = 0x33, p[15] = 0x44;
    }

    /* IMAG */
    for (int i = 0; i < 0x40; i++)
        buf[W_IMAG + i] = (uint8_t)(0x40 + i);
    for (int i = 0; i < 0x20; i++)
        test_put16(buf + W_TLUT + 2 * i, (uint16_t)(0xF001 + i));
    return W_SIZE;
}

#endif
