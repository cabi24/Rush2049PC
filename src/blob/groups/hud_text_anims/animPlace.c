/* animPlace: each player's place ("2/6"), a text blit's draw function (MAT3 hud.c). */
#include "game/game.h"

#define kPractice 1   /* gRace (MAT3 enum kNumRaceTypes) */
#define GAME_STATE_ATTRACT 0x8       /* gGameState: attract mode (the demo) */

typedef struct { s32 x, y; } Pos;
typedef struct { u8 r, g, b, a; } Rgba;

VIEW(s32, gGameState);
#define gGameState gGameState_view
VIEW(s16, gNumCars);
#define gNumCars gNumCars_view

VIEW(Pos[4][4], placePos);
#define placePos placePos_view
VIEW(Rgba[], hud_playerColor);
#define hud_playerColor hud_playerColor_view

/* PC: the place's and the count's digits (docs/port/MORECARS.md 7), n from 1 to 99: one or two. */
static s32 place_digits(char *out, s32 n)
{
    if (n >= 10) {
        out[0] = '0' + n / 10;
        out[1] = '0' + n % 10;
        return 2;
    }
    out[0] = '0' + n;
    return 1;
}

/* Draws each window's place over the number of cars at placePos, the place large in the player's colour and the
 * count smaller in white, each with a black shadow, while the player races (solid, not finished); nothing in attract
 * mode, with one car, in practice, or when the HUD hides the place. Returns 1. */
s32 animPlace(s32 blit)
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

    if ((gGameState & GAME_STATE_ATTRACT) || gNumCars < 2 || gRace == kPractice || !gShowPlace) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    TB_SetJust(kTB_JustCenter, kTB_JustTop);
    if (gNumWindows == 1) {
        HAL_SetFont(8);
    } else {
        HAL_SetFont(5);
    }
    w = HAL_GetCharWidth('8');
    for (i = 0; i < gNumWindows; i++) {
        if (AreWeSolid(i) == 0 || PC_CAR_PLACE_LOCKED(i) == 1) {
            continue;
        }
        x = placePos[gNumWindows - 1][i].x;
        y = placePos[gNumWindows - 1][i].y;
        slash = place_digits(str, PC_CAR_PLACE(i) + 1);
        str[slash] = '/';
        n = slash + 1 + place_digits(str + slash + 1, gNumCars);
        x -= w * slash;
        cw = w;
        for (k = 0; k < n; k++) {
            ch[0] = str[k];
            ch[1] = 0;
            if (k <= slash) {
                if (gNumWindows == 1) {
                    HAL_SetFont(8);
                } else {
                    HAL_SetFont(5);
                }
            } else {
                if (gNumWindows == 1) {
                    HAL_SetFont(5);
                } else {
                    HAL_SetFont(6);
                }
                if (k == slash + 1 && n > slash + 2) {
                    cw = HAL_GetCharWidth('8');   /* the count's font: its second digit */
                }
            }
            TB_SetTextColor(kTB_BlackText);
            TB_DrawText(x + 1, y + 1, ch);
            if (k < slash) {
                HAL_SetTextColor(hud_playerColor[i].r, hud_playerColor[i].g, hud_playerColor[i].b, 255);
            } else {
                TB_SetTextColor(kTB_WhiteText);
            }
            TB_DrawText(x, y, ch);
            x += k > slash ? cw : w;
        }
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
