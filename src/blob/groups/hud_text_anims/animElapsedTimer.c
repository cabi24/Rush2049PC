/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * checkpoint_hit (0x801073D8, 580 B) = animElapsedTimer(struct _tBlit *blt) of hud.c (MAT3 GameCube hud.c
 * animElapsedTimer). Elapsed-time box for window `win` (blit data word): removed (state = 0) when the
 * window does not exist, hidden in attract/when not racing/!gShowElapsed; font 1 or 2; width =
 * 7 digit widths ('8'+1) + 2 colon widths (':'+1, digits fixed-width via HAL_SetFontFixedWidth =
 * HAL_SetFontFixedWidth); position elapsedTimerPos[gNumWindows-1] (elapsedTimerPos); N64: alpha from the colour byte
 * hud_box_color[3] and half18 = 100 (GC: alpha = 128); UpdateBlit = UpdateBlit.
 * Same helper shapes as animLapTime (animLapTime.c): Hidden/gfx_lock/gfx_unlock/font_set inlined, non-static.
 * The alpha store is written before the half18 store (retail's schedule).
 * DISCLOSED: x, w, show (before win) and y (after fixedWidth) are UNUSED locals = exact frame residual (the
 * names follow sibling animLapTime). Retail frame 144 with digitW at sp+124, fixedWidth at sp+119; without them
 * the frame is 128 (8 words differ); x/w/show alone give 136 with the right top offsets.
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

typedef struct Car {
    u8 pad0[239];
    s8 f239;
    u8 padF0[952 - 240];
} Car;

typedef struct Pos {
    s32 x, y;
} Pos;

extern OSMesgQueue text_lock_queue;
extern u32 gGameState;
extern s16 gNumWindows;
extern s8 gShowElapsed;
extern f32 gCheckPtTimeout[];
extern Car gGameCar[];
extern u8 lapTimeBGColor[][4];
extern Pos lapTimePos[][4];
extern Pos elapsedTimerPos[];     /* elapsedTimerPos[gNumWindows - 1] */
extern u8 hud_box_color[4];     /* elapsed-timer colour (alpha byte) */

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void UpdateBlit(void *);
s32 HAL_GetCharWidth(u16 id);
s8 HAL_SetFontFixedWidth(s8 value);
s16 TB_GetFontLineHeight(void);




s32 Hidden_73d8(Blit *blt, s32 hide)
{
    if (blt->hidden != hide) {
        blt->hidden = hide;
        UpdateBlit(blt);
    }
    return blt->hidden;
}

s32 HAL_SetFont(s32 fontID);
s32 animElapsedTimer(Blit *blt)
{
    s32 x;
    s32 w;
    s32 show;
    s32 win;
    s32 digitW;
    s32 colonW;
    s8 fixedWidth;
    s32 y;

    win = blt->data;
    if (Hidden_73d8(blt, (win == 1 && gNumWindows > 1) || (win == 2 && gNumWindows < 2))) {
        blt->state = 0;
        return 1;
    }
    if (Hidden_73d8(blt, (gGameState & 8) || !(gGameState & 0x400000) || !gShowElapsed)) {
        return 1;
    }
    if (gNumWindows == 1) {
        HAL_SetFont(1);
    } else {
        HAL_SetFont(2);
    }
    digitW = HAL_GetCharWidth(56) + 1;
    fixedWidth = HAL_SetFontFixedWidth(0);
    colonW = HAL_GetCharWidth(58) + 1;
    HAL_SetFontFixedWidth(fixedWidth);
    blt->x = elapsedTimerPos[gNumWindows - 1].x - (7 * digitW + 2 * colonW) / 2;
    blt->y = elapsedTimerPos[gNumWindows - 1].y - 1;
    blt->alpha = hud_box_color[3];
    blt->half18 = 100;
    UpdateBlit(blt);
    return 1;
}
