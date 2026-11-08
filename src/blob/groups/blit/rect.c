/*
 * BlitRect(x0, y0, x1, y1, color): clipped solid rectangle (N64-only RDP
 * helper, no arcade ancestor).  Clamp the rectangle to the scissor box
 * (blitXmin/blitYmin .. blitXmax/blitYmax), return if empty, then
 * gDPPipeSync, gDPSetPrimColor(color[0..3]), remember the alpha in
 * blitPrimColor_alpha, BlitColors(color, -1, -1, 0) (texture/colour cache),
 * blit_set_mode(1) (render mode), BlitSetAttribute(0x4000) (set state bit 14:
 * primitive-colour combine), gDPFillRectangle(x0, y0, x1 + 1, y1 + 1),
 * gDPPipeSync, BlitClearAttribute(0x4000).
 *
 * Whole-program context: the real blit_set_mode must be an internal
 * procedure of the same -O3 unit.  Its register summary is why retail keeps
 * the four coordinates in t2-t5 and does not save them around that call
 * (it does around BlitColors and BlitSetAttribute, which are kept).  The
 * group is src/blob/groups/blit (blit_set_mode.c, modes.c unchanged)
 * plus src/blob/func_8008A148.c (unchanged) plus this file.
 * No shaping quirks: SDK GBI macros on the global Gfx *.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

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
#define G_SETPRIMCOLOR 0xFA
#define G_FILLRECT 0xF6
#define gDPNoParam(pkt, cmd) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(cmd, 24, 8); \
    _g->words.w1 = 0; \
}
#define gDPPipeSync(pkt) gDPNoParam(pkt, G_RDPPIPESYNC)
#define gDPSetPrimColor(pkt, m, l, r, g, b, a) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_SETPRIMCOLOR, 24, 8) | _SHIFTL(m, 8, 8) | _SHIFTL(l, 0, 8)); \
    _g->words.w1 = (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8)); \
}
#define gDPFillRectangle(pkt, ulx, uly, lrx, lry) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL((lrx), 14, 10) | _SHIFTL((lry), 2, 10)); \
    _g->words.w1 = (_SHIFTL((ulx), 14, 10) | _SHIFTL((uly), 2, 10)); \
}

extern Gfx *blit_dl;
extern s32 blitXmin;
extern s32 blitYmin;
extern s32 blitXmax;
extern s32 blitYmax;
extern u8 blitPrimColor_alpha;

void BlitColors(u8 *img, s32 mode, s32 pal, s32 bank);
void blit_set_mode(s32 mode);
void BlitSetAttribute(u32 mask);
void BlitClearAttribute(u32 mask);

void BlitRect(s32 x0, s32 y0, s32 x1, s32 y1, u8 *color) {
    if (x0 < blitXmin) {
        x0 = blitXmin;
    }
    if (y0 < blitYmin) {
        y0 = blitYmin;
    }
    if (x1 > blitXmax) {
        x1 = blitXmax;
    }
    if (y1 > blitYmax) {
        y1 = blitYmax;
    }
    if (x1 < x0 || y1 < y0) {
        return;
    }
    gDPPipeSync(blit_dl++);
    gDPSetPrimColor(blit_dl++, 0, 0, color[0], color[1], color[2], color[3]);
    blitPrimColor_alpha = color[3];
    BlitColors(color, -1, -1, 0);
    blit_set_mode(1);
    BlitSetAttribute(0x4000);
    gDPFillRectangle(blit_dl++, x0, y0, x1 + 1, y1 + 1);
    gDPPipeSync(blit_dl++);
    BlitClearAttribute(0x4000);
}
