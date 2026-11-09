/* StartGraphics: begin a frame's display list (MAT3 gfx.c; the N64 side, the RDP set-up of every frame). */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

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

#define G_SETZIMG 0xFE
#define G_MOVEWORD 0xDB
#define G_MW_SEGMENT 0x06
#define G_DL 0xDE
#define G_DL_PUSH 0x00
#define G_SETOTHERMODE_H 0xE3
#define G_GEOMETRYMODE 0xD9
#define G_MDSFT_CYCLETYPE 20
#define G_MDSFT_TEXTFILT 12
#define G_MDSFT_RGBDITHER 6
#define G_CYC_FILL (3 << G_MDSFT_CYCLETYPE)
#define G_TF_POINT (0 << G_MDSFT_TEXTFILT)
#define G_TF_AVERAGE (3 << G_MDSFT_TEXTFILT)
#define G_TF_BILERP (2 << G_MDSFT_TEXTFILT)
#define G_CD_MAGICSQ (0 << G_MDSFT_RGBDITHER)
#define G_CD_BAYER (1 << G_MDSFT_RGBDITHER)
#define G_CD_NOISE (2 << G_MDSFT_RGBDITHER)
#define G_CD_DISABLE (3 << G_MDSFT_RGBDITHER)
#define G_ZBUFFER 0x00000001
#define gDPSetDepthImage(pkt, i) gSetImage(pkt, G_SETZIMG, 0, 0, 1, i)
#define gMoveWd(pkt, index, offset, data) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_MOVEWORD, 24, 8) | _SHIFTL(offset, 0, 16) | _SHIFTL(index, 16, 8); \
    _g->words.w1 = (uintptr_t)(data); \
}
#define gSPSegment(pkt, segment, base) gMoveWd(pkt, G_MW_SEGMENT, (segment) * 4, base)
#define gDma1p(pkt, c, s, l, p) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL((c), 24, 8) | _SHIFTL((p), 16, 8) | _SHIFTL((l), 0, 16)); \
    _g->words.w1 = (uintptr_t)(s); \
}
#define gSPDisplayList(pkt, dl) gDma1p(pkt, G_DL, dl, 0, G_DL_PUSH)
#define gSPSetOtherMode(pkt, cmd, sft, len, data) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(cmd, 24, 8) | _SHIFTL(32 - (sft) - (len), 8, 8) | _SHIFTL((len) - 1, 0, 8)); \
    _g->words.w1 = (unsigned int)(data); \
}
#define gDPSetCycleType(pkt, type) gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_CYCLETYPE, 2, type)
#define gDPSetTextureFilter(pkt, type) gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTFILT, 2, type)
#define gDPSetColorDither(pkt, mode) gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_RGBDITHER, 2, mode)
#define gSPGeometryMode(pkt, c, s) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_GEOMETRYMODE, 24, 8) | _SHIFTL(~(u32)(c), 0, 24); \
    _g->words.w1 = (u32)(s); \
}
#define gSPSetGeometryMode(pkt, word) gSPGeometryMode((pkt), 0, word)

typedef struct {
    PcScTaskShape task;
    u8 *commands;      /* 88 */
    u8 pad5C[32];
    void *colorBuffer; /* 124 */
} FrameBuf;

VIEW(u8 *, frame_dynamic);
#define frame_dynamic frame_dynamic_view

u32 osVirtualToPhysical(void *);
void osViSetSpecialFeatures(u32);
#define OS_VI_DIVOT_ON 0x0010
#define OS_VI_DIVOT_OFF 0x0020
#define OS_VI_DITHER_FILTER_ON 0x0040
#define OS_VI_DITHER_FILTER_OFF 0x0080

#define FRAME_MATRIX_OFFSET 1728   /* where a frame buffer's matrices start (frame_matrix_cursor) */
#define ZBUFFER_CLEAR 0xFFFCFFFC   /* the depth buffer's far value, twice (a fill colour covers two pixels) */
#define SPLIT_LINE_COLOR 0x00010001 /* near black, the lines between the windows of a split screen */

/* Starts the frame's display list in the frame buffer of frame_record_index: the RSP and RDP set-up lists, the depth
 * buffer cleared to far, the colour buffer filled with gBgColor, the lines between the windows of a split screen
 * (gSplitScreen, which ZOID_SplitMenuScreen(0) returns: 2 the grid of quarters, the fourth one black with three
 * windows; 1 two windows stacked, three quarters of the width, the last quarter black), the
 * texture filter and dither the video options chose (video_texture_filter, video_color_dither, the VI's dither
 * filter and divot), then the blitter's and the text's frame (BlitInit, HAL_StartText). EndGraphics ends it. */
void StartGraphics(void) {
    u32 color;
    FrameBuf *frame;
    frame=&frame_records[frame_record_index];
    frame_dynamic=frame->commands;
    frame_matrix_cursor=frame_dynamic+FRAME_MATRIX_OFFSET;
    frame_dl=(Gfx *)(frame_dynamic+PC_GLIST_OFFSET);
    gSPSegment(frame_dl++, 0, 0);
    gSPDisplayList(frame_dl++, rsp_init_dl);
    gSPDisplayList(frame_dl++, rdp_init_dl);
    gDPSetDepthImage(frame_dl++, osVirtualToPhysical(zbuffer));
    gDPPipeSync(frame_dl++);
    gDPSetCycleType(frame_dl++, G_CYC_FILL);
    gDPSetColorImage(frame_dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, screen_width, osVirtualToPhysical(zbuffer));
    gDPSetFillColor(frame_dl++, ZBUFFER_CLEAR);
    gDPFillRectangle(frame_dl++, 0,0,screen_width-1,screen_height-1);
    gSPSetGeometryMode(frame_dl++, G_ZBUFFER);
    color=gBgColor;
    gDPPipeSync(frame_dl++);
    gDPSetColorImage(frame_dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, screen_width, osVirtualToPhysical(frame->colorBuffer));
    gDPSetFillColor(frame_dl++, color);
    gDPFillRectangle(frame_dl++, 0,0,screen_width-1,screen_height-1);
    switch(ZOID_SplitMenuScreen(0)) {
    case 2:
        if(gNumWindows==3) {
            gDPPipeSync(frame_dl++);
            gDPSetFillColor(frame_dl++, SPLIT_LINE_COLOR);
            gDPFillRectangle(frame_dl++, screen_width/2,screen_height/2,screen_width-1,screen_height-1);
        }
        gDPPipeSync(frame_dl++);
        gDPSetFillColor(frame_dl++, SPLIT_LINE_COLOR);
        gDPFillRectangle(frame_dl++, screen_width/2-1,0,screen_width/2,screen_height-1);
        gDPPipeSync(frame_dl++);
        gDPFillRectangle(frame_dl++, 0,screen_height/2-1,screen_width-1,screen_height/2);
        break;
    case 1:
        gDPPipeSync(frame_dl++);
        gDPSetFillColor(frame_dl++, SPLIT_LINE_COLOR);
        gDPFillRectangle(frame_dl++, (screen_width*3)/4,0,screen_width-1,screen_height-1);
        gDPPipeSync(frame_dl++);
        gDPFillRectangle(frame_dl++, 0,screen_height/2-1,screen_width-1,screen_height/2);
        gDPPipeSync(frame_dl++);
        gDPFillRectangle(frame_dl++, 0,0,1,screen_height-1);
        break;
    }
    gDPPipeSync(frame_dl++);
    switch(video_texture_filter) {
    case 0:gDPSetTextureFilter(frame_dl++, G_TF_BILERP);break;
    case 1:gDPSetTextureFilter(frame_dl++, G_TF_AVERAGE);break;
    case 2:gDPSetTextureFilter(frame_dl++, G_TF_POINT);break;
    }
    switch(video_color_dither) {
    case 0:gDPSetColorDither(frame_dl++, G_CD_DISABLE);break;
    case 1:gDPSetColorDither(frame_dl++, G_CD_BAYER);break;
    case 2:gDPSetColorDither(frame_dl++, G_CD_MAGICSQ);break;
    case 3:gDPSetColorDither(frame_dl++, G_CD_NOISE);break;
    }
    switch(video_dither_filter) {
    case 0:osViSetSpecialFeatures(OS_VI_DITHER_FILTER_OFF);break;
    case 1:osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON);break;
    }
    if(video_divot)osViSetSpecialFeatures(OS_VI_DIVOT_ON);
    else osViSetSpecialFeatures(OS_VI_DIVOT_OFF);
    BlitInit();
    HAL_StartText();
}
