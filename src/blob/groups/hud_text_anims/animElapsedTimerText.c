/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * lap_complete (0x8010761C, 1,232 B) -- historical label; = GC hud.c animElapsedTimerText (static int (void);
 * GC locals t, digits[9], str[2], digitW, colonW, fixedWidth, x, y, digitIdx; gModelTime,
 * gShowElapsed, elapsedTimerPos, HAL_SetFontFixedWidth,
 * camera_shake_update = HAL_GetCharWidth, gJust = TB_SetJust's pair). Draws the race clock as "mm:ss.hh"
 * with a shadow pass. drafts, match (unit EQUAL, 0 locked bodies differ). No devices.
 * Levers: GC local list/order and statement forms (digitIdx index loop, `str[0] = digits[digitIdx]` then the
 * ':'/'.' test on str[0] through `if ((str[0] = digits[digitIdx]) != ':' && ...)`, `str[1] = 0;` before
 * y/x, y assigned before x); s32 y with TB_DrawText(s16, s16, char *) (retail narrows both at every call);
 * the TB_SetJust pair written by an inlined pair_set(s32, s32) storing .second then .first on an s32 pair (as
 * the matched sibling animLapCount; the s32 1 joins the gNumWindows == 1 / osRecvMesg constant piece in v0,
 * and the two inlined sites size the 184-byte frame). gfx_lock/gfx_unlock/font_set/pair_set are the family's
 * inlined helpers, non-static (suffix _761C) so umerge leaves no deleted-procedure stubs (as animLapCount.c).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;
typedef struct { s32 first, second; } Pair;
typedef struct { s32 x, y; } Pos;
extern OSMesgQueue text_lock_queue;
extern s32 gGameState;
extern s8 gShowElapsed;
extern f32 gModelTime;
extern s16 gNumWindows;
extern Pair gJust;
extern Pos elapsedTimerPos[];
extern s32 osRecvMesg(OSMesgQueue *, void **, s32);
extern s32 osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32 selection);
extern void HAL_SetTextDepth(f32);
extern s32 HAL_GetCharWidth(u16 id);
extern s8 HAL_SetFontFixedWidth(s8 value);
extern void TB_SetTextColor(s32);
extern void TB_DrawText(s16, s16, char *);





s32 HAL_SetFont(s32 fontID);
void TB_SetJust(u32 first, u32 second);
s32 animElapsedTimerText(s32 arg0)
{
    s32 t;
    char digits[9];
    char str[2];
    s32 digitW;
    s32 colonW;
    s8 fixedWidth;
    s32 x;
    s32 y;
    s32 digitIdx;

    if ((gGameState & 8) || !(gGameState & 0x400000) || !gShowElapsed) {
        return 1;
    }
    t = gModelTime * 1000.0f;
    if (t < 0) t = 0;
    digits[0] = t / 600000 % 6 + '0';
    digits[1] = t / 60000 % 10 + '0';
    digits[2] = ':';
    digits[3] = t / 10000 % 6 + '0';
    digits[4] = t / 1000 % 10 + '0';
    digits[5] = '.';
    digits[6] = t / 100 % 10 + '0';
    digits[7] = t / 10 % 10 + '0';
    digits[8] = t % 10 + '0';
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 3);
    if (gNumWindows == 1) {
        HAL_SetFont(1);
    } else {
        HAL_SetFont(2);
    }
    digitW = HAL_GetCharWidth('8') + 1;
    fixedWidth = HAL_SetFontFixedWidth(0);
    colonW = HAL_GetCharWidth(':') + 1;
    HAL_SetFontFixedWidth(fixedWidth);
    str[1] = 0;
    y = elapsedTimerPos[gNumWindows - 1].y;
    x = elapsedTimerPos[gNumWindows - 1].x - (digitW * 5 + colonW * 2) / 2;
    for (digitIdx = 0; digitIdx < 8; digitIdx++) {
        if ((str[0] = digits[digitIdx]) != ':' && str[0] != '.') {
            str[0] = '8';
            TB_SetTextColor(0);
            TB_DrawText(x, y, str);
            str[0] = digits[digitIdx];
        }
        TB_SetTextColor(1);
        TB_DrawText(x, y, str);
        if (digitIdx == 1 || digitIdx == 2 || digitIdx == 4 || digitIdx == 5) {
            x += (digitW + colonW) / 2;
        } else {
            x += digitW;
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
