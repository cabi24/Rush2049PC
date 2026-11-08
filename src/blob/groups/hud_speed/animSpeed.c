/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animSpeed (0x800EFEA8, 600 B; historical label), : an N64 hud.c blit animation callback
 * (the function right before ShowHUD), same family as the matched animLapTime (animLapTime.c) and
 * animPlace: the blit's data word packs the window (low nibble) and a display mode (bits 4..7: 1 = single-window
 * only, 2 = split-screen only). Hidden when the mode does not fit gNumWindows or the window does not
 * exist (then state = 0), or when gShowSpeed is clear or the window's car (gGameCar, 952 B, indexed
 * by model[win].slot: D_8014A250, 0x808 B, s16 at +1990) has place_locked (+239) == 1. Otherwise font 4 (one
 * window) or 1 under the font lock, digit width = camera_shake_update('8') (HAL_GetCharWidth), position from the
 * per-window-count table speedPos[4][4] {x, y}: x = pos.x - 2 * digitW - 2, y = pos.y + 1 in single-window,
 * alpha = hud_box_alpha, half18 = 100, UpdateBlit. MAT3 identity open (the GC hud.c has no
 * blit per window for this element); structure from the matched N64 siblings.
 * Needs HAL_SetFontInternal's IPA convention (parameter in s2, s0/s1/s3 unsaved): group context ctx_gr3_b.c.
 * Shaping (each verified necessary): Hidden / gfx_lock / gfx_unlock / font_set are non-static inlined helpers as
 * in animLapTime; the single-window y step is a `?:` (IDO's li v0,1 / move v0,zero arms); alpha is stored before
 * half18 (ugen temp order: retail t7 = alpha, t8 = 100).
 * Disclosed frame residual: x, y, yoff, w and h are UNUSED named locals (5 slots, 20 bytes). Retail frame 144
 * with win's home at sp+136 (one named slot above it) and the inlined font_set homes at 76/52; without the five
 * the frame is 120 (win at 116); with x, y, yoff only it is 136 (win at 128).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    uintptr_t word0;
    void *rgba;
    uintptr_t word8;
    u16 half12;
    s16 x, y;
    u16 half18;
    s16 w, h;
    u8 alpha, byte25;
    s8 hidden;
    u8 byte27;
    s16 left, right, top, bottom;
    u32 word36;
    intptr_t state;     /* AnimFunc */
    s32 data;
} Blit;

typedef struct Model { u8 pad0[1990]; s16 slot; u8 pad7C8[2056 - 1992]; } Model;   /* model[] 0x808 B */
typedef struct Car { u8 pad0[239]; s8 f239; u8 padF0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 padF0_308[0x380 - 0x308]; void *input; u8 padF0_384[0x3B8 - 0x384]; } Car;
typedef struct Pos { s32 x, y; } Pos;

extern OSMesgQueue text_lock_queue;
extern s16 gNumWindows;
extern s8 gShowSpeed;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern u8 hud_box_alpha;
extern Pos speedPos[][4];

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void UpdateBlit(void *);
s32 HAL_GetCharWidth(u16 id);





s32 HAL_SetFont(s32 fontID);
s32 Hidden(Blit *blt, s32 hide);
s32 animSpeed(Blit *blt)
{
    s32 x;          /* unused (frame residual) */
    s32 win;
    s32 mode;
    s32 hide;
    s32 digitW;
    s32 y;          /* unused (frame residual) */
    s32 yoff;       /* unused (frame residual) */
    s32 w;          /* unused (frame residual) */
    s32 h;          /* unused (frame residual) */

    win = blt->data & 15;
    mode = ((u32)blt->data & 0xF0) >> 4;
    hide = (mode == 1 && gNumWindows >= 2) || (mode == 2 && gNumWindows < 2) || win >= gNumWindows;
    if (Hidden(blt, hide)) {
        blt->state = 0;
        return 1;
    }
    hide = !gShowSpeed || PC_CAR_FINISHED(PC_MODEL_SLOT(win)) == 1;
    if (Hidden(blt, hide)) {
        return 1;
    }
    if (gNumWindows == 1) {
        HAL_SetFont(4);
    } else {
        HAL_SetFont(1);
    }
    digitW = HAL_GetCharWidth(56);
    blt->x = speedPos[gNumWindows - 1][win].x - digitW * 2 - 2;
    blt->y = speedPos[gNumWindows - 1][win].y + ((gNumWindows == 1) ? 1 : 0);
    blt->alpha = hud_box_alpha;
    blt->half18 = 100;
    UpdateBlit(blt);
    return 1;
}
