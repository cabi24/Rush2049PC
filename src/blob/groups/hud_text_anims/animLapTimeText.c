/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_801084D4 (0x801084D4, 1500 B) = GC hud.c animLapTimeText (MAT3 decomp/src/GameSource/src/hud.c
 * animLapTimeText; GC locals t, remaining, digits[9], str[2], digitW, colonW, x, y, digitIdx, slot,
 * fixedWidth). Per-window lap split "m:ss.hhh" with a shadow pass, fading out over the last 3 s of
 * gCheckPtTimeout. gCheckPtTimeout = gCheckPtTimeout, gLapTime = gLapTime, gShowElapsed = gShowElapsed,
 * lapTimePos = lapTimePos, gPauseState = gPauseState, gFrameSeconds = gFrameSeconds (volatile, as MAT3),
 * HAL_SetTextTranslucency = HAL_SetTextTranslucency, TB_DrawText = TB_DrawText, TB_SetTextColor = TB_SetTextColor,
 * HAL_GetCharWidth = camera_shake_update, HAL_SetFontFixedWidth = HAL_SetFontFixedWidth, HAL_SetFont =
 * HAL_SetFontInternal (s2 IPA parameter), TB_SetJust pair = gJust.
 * N64 differences from GC: an unused s32 parameter (stored to its home, as the sibling animElapsedTimerText);
 * `remaining` declared last and assigned after the continue test; the loop's zero is spelled `0.f` (a different
 * uopt constant from HAL_SetTextDepth's 0.0f: retail hoists only the loop zero); gLapTime * 1000.0f and
 * remaining * 255.0f operand order. No shaping devices. Helpers gfx_lock/gfx_unlock/font_set/pair_set are the
 * family's inlined helpers, non-static (suffix _84D4), not on keep (stub rule).
 */typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;
typedef struct { s32 first, second; } Pair;
typedef struct { s32 x, y; } Pos;

typedef struct Car {
    u8 pad0[239];
    s8 place_locked;
    u8 padF0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 padF0_308[0x380 - 0x308]; void *input; u8 padF0_384[0x3B8 - 0x384];
} Car;

extern OSMesgQueue text_lock_queue;
extern u32 gGameState;
extern s8 gShowElapsed;
extern s16 gNumWindows;
extern Pair gJust;
extern f32 gCheckPtTimeout[];
extern f32 gLapTime[];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Pos lapTimePos[][4];
extern s32 gPauseState;
extern volatile f32 gFrameSeconds;

extern s32 osRecvMesg(OSMesgQueue *, void **, s32);
extern s32 osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32 selection);
extern void HAL_SetTextDepth(f32);
extern s32 HAL_GetCharWidth(u16 id);
extern s8 HAL_SetFontFixedWidth(s8 value);
extern void TB_SetTextColor(s32);
extern void TB_DrawText(s16, s16, u8 *);
extern void HAL_SetTextTranslucency(f32);

void gfx_lock_84D4(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_84D4(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

s32 font_set_84D4(s32 font)
{
    s32 old;

    gfx_lock_84D4();
    old = HAL_SetFontInternal(font);
    gfx_unlock_84D4();
    return old;
}

void pair_set_84D4(s32 first, s32 second)
{
    gJust.second = second;
    gJust.first = first;
}

s32 animLapTimeText(s32 arg0)
{
    s32 t;
    u8 digits[9];
    u8 str[2];
    s32 digitW;
    s32 colonW;
    s32 x;
    s32 y;
    s32 digitIdx;
    s32 slot;
    s8 fixedWidth;
    f32 remaining;

    if ((gGameState & 8) || !gShowElapsed) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    pair_set_84D4(1, 3);
    if (gNumWindows == 1) {
        font_set_84D4(1);
    } else {
        font_set_84D4(2);
    }
    digitW = HAL_GetCharWidth('8') + 1;
    fixedWidth = HAL_SetFontFixedWidth(0);
    colonW = HAL_GetCharWidth(':') + 1;
    HAL_SetFontFixedWidth(fixedWidth);
    for (slot = 0; slot < gNumWindows; slot++) {
        if (gPauseState == 0) {
            gCheckPtTimeout[slot] -= gFrameSeconds;
        }
        if (gCheckPtTimeout[slot] <= 0.f || PC_CAR_FINISHED(slot) == 1) {
            gCheckPtTimeout[slot] = 0.f;
            continue;
        }
        remaining = gCheckPtTimeout[slot];
        t = gLapTime[slot] * 1000.0f;
        if (t < 0) {
            t = 0;
        }
        digits[0] = t / 600000 + '0';
        digits[1] = t / 60000 % 10 + '0';
        digits[2] = ':';
        digits[3] = t / 10000 % 6 + '0';
        digits[4] = t / 1000 % 10 + '0';
        digits[5] = '.';
        digits[6] = t / 100 % 10 + '0';
        digits[7] = t / 10 % 10 + '0';
        digits[8] = t % 10 + '0';
        y = lapTimePos[gNumWindows - 1][slot].y;
        str[1] = 0;
        if (remaining < 3.0f) {
            HAL_SetTextTranslucency((s32)(remaining * 255.0f / 3.0f));
        }
        x = lapTimePos[gNumWindows - 1][slot].x - (digitW * 6 + colonW * 2) / 2;
        for (digitIdx = 0; digitIdx < 9; digitIdx++) {
            str[0] = digits[digitIdx];
            if (digits[digitIdx] != ':' && digits[digitIdx] != '.') {
                str[0] = '8';
                TB_SetTextColor(0);
                TB_DrawText(x, y, str);
                str[0] = digits[digitIdx];
            }
            TB_SetTextColor(22);
            TB_DrawText(x, y, str);
            if (digitIdx == 1 || digitIdx == 2 || digitIdx == 4 || digitIdx == 5) {
                x += (digitW + colonW) / 2;
            } else {
                x += digitW;
            }
        }
        HAL_SetTextTranslucency(-1.0f);
    }
    pair_set_84D4(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
