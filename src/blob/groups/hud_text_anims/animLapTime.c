/*
 * func_80108154 (0x80108154, 896 B) = animLapTime(struct _tBlit *blt) of hud.c (GC rush_d.elf hud.c animLapTime:
 * locals w, digitW, colonW, win, show, fixedWidth; float 192). Lap-time box for window `win` (blit data word):
 * hidden past the window count or when gGameState bit 3 / !gShowElapsed / lap timer gCheckPtTimeout[win] <= 0 / car
 * f239 == 1; font 1 (one window) or 2; width = 7 digit widths ('8'+1) + 2 colon widths (':'+1) with
 * fixed-width digits forced (HAL_SetFontFixedWidth); fade alpha lapTimeBGColor[win][3] = timer*192/3 below 3 s.
 * Shaping (disclosed): Hidden/gfx_lock/gfx_unlock/font_set are inlined helpers (non-static: no
 * deleted-procedure stubs); `x` is unused: exact frame residual (one 4-byte named slot; retail frame 152 with
 * the locals at 148..127; without `x` the frame stays 152 but every named slot moves 4 B, 10 words differ).
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
    u8 padF0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 padF0_308[0x380 - 0x308]; void *input; u8 padF0_384[0x3B8 - 0x384];
} Car;

typedef struct Pos {
    s32 x, y;
} Pos;

extern OSMesgQueue text_lock_queue;
extern u32 gGameState;
extern s16 gNumWindows;
extern s8 gShowElapsed;
extern f32 gCheckPtTimeout[];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern u8 lapTimeBGColor[][4];
extern Pos lapTimePos[][4];

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void UpdateBlit(void *);
s32 HAL_GetCharWidth(u16 id);
s8 HAL_SetFontFixedWidth(s8 value);
s16 TB_GetFontLineHeight(void);





s32 HAL_SetFont(s32 fontID);
s32 Hidden(Blit *blt, s32 hide);
s32 animLapTime(Blit *blt)
{
    s32 x;
    s32 w;
    s32 digitW;
    s32 colonW;
    s32 show;
    s32 win;
    s8 fixedWidth;

    win = blt->data;
    if (win >= gNumWindows) {
        blt->state = 0;
        Hidden(blt, 1);
        return 1;
    }
    show = (gGameState & 8) || !gShowElapsed || gCheckPtTimeout[win] <= 0.0f || PC_CAR_FINISHED(win) == 1;
    if (Hidden(blt, show)) {
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
    if (gCheckPtTimeout[win] < 3.0f) {
        lapTimeBGColor[win][3] = gCheckPtTimeout[win] * 192.0f / 3.0f;
    } else {
        lapTimeBGColor[win][3] = 192;
    }
    w = digitW * 7 + colonW * 2;
    blt->x = lapTimePos[gNumWindows - 1][win].x - w / 2;
    blt->w = w;
    blt->y = lapTimePos[gNumWindows - 1][win].y;
    blt->h = TB_GetFontLineHeight();
    blt->alpha = lapTimeBGColor[win][3];
    blt->half18 = 100;
    blt->rgba = lapTimeBGColor[win];
    UpdateBlit(blt);
    return 1;
}
