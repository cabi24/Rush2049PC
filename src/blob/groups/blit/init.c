/*
 * BlitInit (0x800A4934, 400 bytes): begin a double-buffered display list.
 * Historical name; this is N64 graphics-state initialization, not audio.
 * No arcade ancestor: N64-specific RDP/RSP state-cache and GBI setup.
 *
 * Matches in the real -O3 group described by recipe.json. Keep this public
 * no-argument entry and the existing gfx_modes roots; blit_set_mode remains
 * internal so its genuine register summary preserves the height address.
 * The accepted blit_load_texture.c owns blit_cache_tile as s64 in that same group.
 * Do not replace its definition with an extern-only unit or invent a second
 * object owner here. No accepted source file is modified.
 *
 * The four SDK-style macros each have their own block-local Gfx pointer.
 * Their source lifetimes reproduce the retail allocator without fake locals,
 * keepers, assembly, volatile steering, or unrelated callers.
 */
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned char u8;
typedef signed short s16;
typedef signed long long s64;
typedef struct { u32 w0; uintptr_t w1; } Gwords;
typedef union {
    Gwords words;
    s64 force_structure_alignment;
} Gfx;

/* SDK GBI forms, using the same self-contained types as gfx_modes. */
#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define G_SETBLENDCOLOR 0xF9
#define G_RDPPIPESYNC 0xE7
#define G_SETOTHERMODE_H 0xE3
#define G_GEOMETRYMODE 0xD9
#define G_MDSFT_TEXTPERSP 19
#define G_TP_NONE (0 << G_MDSFT_TEXTPERSP)

#define gDPSetColor(pkt, c, d) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(c, 24, 8); \
    _g->words.w1 = (unsigned int)(d); \
}
#define gDPSetBlendColor(pkt, r, g, b, a) \
    gDPSetColor(pkt, G_SETBLENDCOLOR, \
        _SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | \
        _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8))
#define gDPPipeSync(pkt) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_RDPPIPESYNC, 24, 8); \
    _g->words.w1 = 0; \
}
#define gSPSetOtherMode(pkt, cmd, sft, len, data) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(cmd, 24, 8) | \
        _SHIFTL(32 - (sft) - (len), 8, 8) | _SHIFTL((len) - 1, 0, 8)); \
    _g->words.w1 = (unsigned int)(data); \
}
#define gDPSetTexturePersp(pkt, type) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTPERSP, 1, type)
#define gSPGeometryMode(pkt, c, s) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_GEOMETRYMODE, 24, 8) | _SHIFTL(~(u32)(c), 0, 24); \
    _g->words.w1 = (u32)(s); \
}
#define gSPClearGeometryMode(pkt, c) gSPGeometryMode(pkt, c, 0)

extern Gfx *blit_dl;
extern Gfx blit_display_lists[2][2400];
/* PC: the two text display lists hold 16-byte host Gfx; blit_display_lists is sized for 8-byte commands. */
Gfx pc_text_dl[2][2400];
#define TEXT_DL pc_text_dl
extern u32 blitAttribute;
extern s32 blitXmin, blitYmin, blitXmax;
extern s32 screen_width, screen_height, blitYmax;
extern s16 blitZDist;
extern u8 blitPrimColor_alpha;
extern void *blit_cache_image;
extern intptr_t blitColors;
extern s32 blitDLnum, blitMode, blitLUT;
extern s64 blit_cache_tile;
extern void blit_set_mode(s32);
extern void BlitSetAttribute(u32);

void BlitInit(void)
{
    if (blit_dl == 0) {
        blitAttribute = 0;
        blitXmin = 0;
        blitYmin = 0;
        blitXmax = screen_width - 1;
        blitYmax = screen_height - 1;
        blitZDist = 0;
        blitPrimColor_alpha = 255;
        blit_cache_image = 0;
        blit_cache_tile = 0;
        if (++blitDLnum >= 2) blitDLnum = 0;
        blit_dl = TEXT_DL[blitDLnum];
        blitMode = -1;
        blitLUT = -1;
        blitColors = 0;
        gDPSetBlendColor(blit_dl++, 0, 0, 0, 16);
        gDPPipeSync(blit_dl++);
        gDPSetTexturePersp(blit_dl++, G_TP_NONE);
        gSPClearGeometryMode(blit_dl++, -1);
        blit_set_mode(1);
        if (screen_height >= 221) BlitSetAttribute(0x8000);
    }
}
