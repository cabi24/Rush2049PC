/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800F7448(u16 color): clear the current frame buffer to a 16-bit fill colour. Appends to the global
 * display-list pointer D_801497C8: pipe sync; SetColorImage(RGBA16, width D_8002AFC0,
 * osVirtualToPhysical(D_80156C5C[D_8015F72D].buf)) (128-byte frame-buffer records, volatile s8 index);
 * SetFillColor(color << 16 | color); FillRectangle(0, 0, width - 1, height D_8002AFC4 - 1); pipe sync.
 * SDK GBI macros. N64-only; no arcade ancestor.
 * Shaping: the fill word is `((u32)color << 16) | color` -- without the cast uopt gives `color` a register web
 * (a2) and the _g temp ring shifts by one (t1 for t0); a `fill` local is worse.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
typedef float f32;

typedef struct {
    u32 w0;
    uintptr_t w1;
} Gwords;

typedef union {
    Gwords words;
    long long int force_structure_alignment;
} Gfx;

#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define G_RDPPIPESYNC 0xE7
#define G_SETCIMG 0xFF
#define G_SETFILLCOLOR 0xF7
#define G_FILLRECT 0xF6
#define G_IM_FMT_RGBA 0
#define G_IM_SIZ_16b 2

#define gSetImage(pkt, cmd, fmt, siz, width, i) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(cmd, 24, 8) | _SHIFTL(fmt, 21, 3) | _SHIFTL(siz, 19, 2) | _SHIFTL((width) - 1, 0, 12); \
    _g->words.w1 = (uintptr_t)(i); \
}
#define gDPSetColorImage(pkt, f, s, w, i) gSetImage(pkt, G_SETCIMG, f, s, w, i)
#define gDPNoParam(pkt, cmd) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(cmd, 24, 8); \
    _g->words.w1 = 0; \
}
#define gDPPipeSync(pkt) gDPNoParam(pkt, G_RDPPIPESYNC)
#define gDPSetColor(pkt, c, d) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(c, 24, 8); \
    _g->words.w1 = (unsigned int)(d); \
}
#define gDPSetFillColor(pkt, d) gDPSetColor(pkt, G_SETFILLCOLOR, d)
#define gDPFillRectangle(pkt, ulx, uly, lrx, lry) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL((lrx), 14, 10) | _SHIFTL((lry), 2, 10)); \
    _g->words.w1 = (_SHIFTL((ulx), 14, 10) | _SHIFTL((uly), 2, 10)); \
}

#ifdef TARGET_PC
/* PC: D_80156C5C is the colour buffer of frame record 0 (D_80156BE0 + 0x7C), and a record seen from there runs
 * into the next record's scheduler task, the native OSScTask (pointer fields where it has them): 208 bytes a
 * record, as the record views have it (docs/port/POINTERS.md 10). */
typedef struct { void *next; u32 state, flags; void *fb; u32 type, tflags; void *boot; u32 boot_size;
                 void *ucode; u32 ucode_size; void *udata; u32 udata_size; void *stack; u32 stack_size;
                 void *out; void *out_size; void *data; u32 data_size; void *yield; u32 yield_size;
                 void *msgQ; void *msg; } PcScTaskShape;
typedef struct { void *buf; PcScTaskShape task; u8 *commands; s16 n; u8 pad[30]; } FrameBuf;
#else
typedef struct { void *buf; u8 pad[124]; } FrameBuf;
#endif

extern Gfx *D_801497C8;
extern s32 D_8002AFC0;
extern s32 D_8002AFC4;
extern volatile s8 D_8015F72D;
extern FrameBuf D_80156C5C[];
u32 osVirtualToPhysical(void *);

void func_800F7448(u16 color)
{
    gDPPipeSync(D_801497C8++);
    gDPSetColorImage(D_801497C8++, G_IM_FMT_RGBA, G_IM_SIZ_16b, D_8002AFC0, osVirtualToPhysical(D_80156C5C[D_8015F72D].buf));
    gDPSetFillColor(D_801497C8++, ((u32)color << 16) | color);
    gDPFillRectangle(D_801497C8++, 0, 0, D_8002AFC0 - 1, D_8002AFC4 - 1);
    gDPPipeSync(D_801497C8++);
}
