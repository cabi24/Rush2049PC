/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animPlace (0x80107AF4, 1,000 B; historical label race_finish) = GC hud.c `static int animPlace(void)` (GC locals
 * digits[3], str[2], digitW, x, y, digitIdx, slot). Per-player race position "P/N" HUD (place + 1, '/', car count),
 * drawn glyph by glyph with a shadow pass at (x+1, y+1); the place digit in the player colour hud_playerColor[i].
 * GC-derived names: AreWeSolid, placePos, hud_playerColor = playerColor, gShowPlace =
 * gShowPlace, camera_shake_update = HAL_GetCharWidth, HAL_SetTextDepth, gJust = TB_SetJust's
 * pair, TB_SetTextColor, TB_DrawText = TB_DrawText (s16, s16, char *).
 * EQUAL in the whole-program unit. Same TU as animElapsedTimerText/animLapCount
 * (credits_scroll_grp, hud.c): helpers *_7AF4 are non-static and not kept (no deleted-procedure stubs; the
 * locked stub hud_inlined_stub before animPlace stays untouched).
 * Shaping (disclosed):
 *  - DEVICE: compiled-out `if (i) {}` at the end of the outer loop body (raises i over the int-1 constant web so i
 *    gets s8 as in retail; without it 87 rows).
 * - `x = placePos.x; .; x -= w;` split assignment (split): the placePos address expression is then
 *    numbered before y (retail address in v0, y in v1) and the loads schedule y, place, x, w as retail.
 *  - pair_set_7AF4 writes the global directly (.second then .first, as animElapsedTimerText's pair_set_761C) at
 * both sites; the `Pair *p` form put one more spill home before (s16)(y+1)/(s16)y (homes 72/68 vs 76/72).
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
typedef struct { u8 r, g, b, a; } Rgba;
typedef struct {
    u8 pad000[238];
    s8 place;        /* 238 */
    s8 finished;     /* 239 */
    u8 pad0F0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 pad0F0_308[0x380 - 0x308]; void *input; u8 pad0F0_384[0x3B8 - 0x384];
} Player952;
extern OSMesgQueue text_lock_queue;
extern s32 gGameState;
extern s16 gNumCars;
extern s32 gRace;
extern s8 gShowPlace;
extern s16 gNumWindows;
extern Pair gJust;
extern Pos placePos[4][4];
extern Rgba hud_playerColor[];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s32 osRecvMesg(OSMesgQueue *, void **, s32);
extern s32 osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32 selection);
extern void HAL_SetTextDepth(f32);
extern s32 HAL_GetCharWidth(u16 id);
extern void TB_SetTextColor(s32);
extern void TB_DrawText(s16, s16, char *);
extern s32 AreWeSolid(s16);
extern void HAL_SetTextColor(s32, s32, s32, s32);

void gfx_lock_7AF4(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_7AF4(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

s32 font_set_7AF4(s32 font)
{
    s32 old;

    gfx_lock_7AF4();
    old = HAL_SetFontInternal(font);
    gfx_unlock_7AF4();
    return old;
}

void pair_set_7AF4(s32 first, s32 second)
{
    gJust.second = second;
    gJust.first = first;
}

/* PC: the place's and the count's digits (docs/port/MORECARS.md 7), n from 1 to 99: one or two. */
static s32 place_digits_7AF4(char *out, s32 n)
{
    if (n >= 10) {
        out[0] = '0' + n / 10;
        out[1] = '0' + n % 10;
        return 2;
    }
    out[0] = '0' + n;
    return 1;
}

/* PC: animPlace with numbers of two digits (docs/port/MORECARS.md 7). The N64 makes the place and the car count one
 * character each, '1' + place and '0' + cars, so a race of 10 or more cars gave ':', ';' and '<', which the font has
 * no glyph for: the place vanished, and in split screen a '/' was left. Here each number has its digits. A place of
 * two digits starts one digit further left, so the '/' and the count's first digit stay where the N64 draws them; a
 * count's second digit follows its first by a digit of the count's own font. Up to 9 cars this makes the N64's calls
 * in the N64's order, at the same places. */
s32 animPlace(s32 arg0)
{
    char str[8];
    char ch[2];
    s32 w;
    s32 cw;
    s32 i;
    s32 x;
    s32 y;
    s32 k;
    s32 n;
    s32 slash;

    if ((gGameState & 8) || gNumCars < 2 || gRace == 1 || !gShowPlace) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    pair_set_7AF4(1, 3);
    if (gNumWindows == 1) {
        font_set_7AF4(8);
    } else {
        font_set_7AF4(5);
    }
    w = HAL_GetCharWidth('8');
    for (i = 0; i < gNumWindows; i++) {
        if (AreWeSolid(i) == 0 || PC_CAR_FINISHED(i) == 1) {
            continue;
        }
        x = placePos[gNumWindows - 1][i].x;
        y = placePos[gNumWindows - 1][i].y;
        slash = place_digits_7AF4(str, PC_CAR_PLACE(i) + 1);
        str[slash] = '/';
        n = slash + 1 + place_digits_7AF4(str + slash + 1, gNumCars);
        x -= w * slash;
        cw = w;
        for (k = 0; k < n; k++) {
            ch[0] = str[k];
            ch[1] = 0;
            if (k <= slash) {
                if (gNumWindows == 1) {
                    font_set_7AF4(8);
                } else {
                    font_set_7AF4(5);
                }
            } else {
                if (gNumWindows == 1) {
                    font_set_7AF4(5);
                } else {
                    font_set_7AF4(6);
                }
                if (k == slash + 1 && n > slash + 2) {
                    cw = HAL_GetCharWidth('8');   /* the count's font: its second digit */
                }
            }
            TB_SetTextColor(0);
            TB_DrawText(x + 1, y + 1, ch);
            if (k < slash) {
                HAL_SetTextColor(hud_playerColor[i].r, hud_playerColor[i].g, hud_playerColor[i].b, 255);
            } else {
                TB_SetTextColor(1);
            }
            TB_DrawText(x, y, ch);
            x += k > slash ? cw : w;
        }
    }
    pair_set_7AF4(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
