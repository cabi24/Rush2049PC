/* The cheat menu (MAT3 cheatscr.c), GS_CHEAT, opened from the start menu once its code has been entered: 21 cheats
 * (MAT3 23). A cheat is switched with left and right once the player has entered its own code on this screen
 * (gCheatActive; CheckForEgg follows the code of the selected row); each row writes its game flag and mirrors it in
 * gCheatState, which the screen draws. */
#include "game/game.h"

typedef struct Blit Blit;
typedef struct MultiBlit {
    const char *texname;
    s16 dulx, duly, width, height, top, bot, left, right;
    u32 zdepth, alpha;
    s32 (*animfunc)(Blit *);
    u32 animid;
} MultiBlit;

VIEW(const MultiBlit[], cheatscr_multiblit);
#define cheatscr_multiblit cheatscr_multiblit_view
VIEW(s16[], gCheatState);
#define gCheatState gCheatState_view

VIEW(u32, gActiveAutoEdges);
#define gActiveAutoEdges gActiveAutoEdges_view

extern Blit *NewMultiBlit(s16, s16, const MultiBlit *, s16);
extern void RemoveBlit(Blit *);

/* PC: game.cheats_unlock (port/config/cheats.c, docs/port/FRONTEND.md "The cheat screen in the main menu"): every
   cheat is available while it is on, its code not needed: the tests of gCheatActive[cheat] that let LEFT and RIGHT
   switch it and draw it bright take it as set. The flags aren't written; off, the N64's. */
int pc_cheat_active(int active);
#define PC_CHEAT_ACTIVE(a) pc_cheat_active(a)

#define CHEAT_COUNT 21
#define CHEAT_FRAME_SCALE 18        /* the one row shown as a number */
#define CHEAT_FOG_COLOR 20          /* the fog colour row: gFogColor 0, or 19-26 */
#define FOG_COLOR_BASE 18           /* gFogColor of colour n is n + 18 */
#define FOG_COLORS 8
#define ROWS_SHOWN 13               /* the list shows the selected row and six each side, wrapping round */
/* The menu sounds (PlayStartABSound's and PlayUDSound's effects). */
#define SND_MENU_START 46
#define SND_MENU_BACK 38
#define SND_MENU_DOWN 39
#define SND_MENU_UP 40

/* The screen's frame. Start or B leaves (to the start menu; a soft reset to the title). Up and down
 * (auto-repeating) move round the cheats and start watching for the new row's code. Left or right changes the
 * selected cheat if it is unlocked: most flip a flag, the mass, suicide mode and frame scale step through their
 * values, the fog colour through none and eight colours. A cheat not yet unlocked is unlocked when its code is
 * entered. */
void updateCheatScreen(void)
{
    if (!cheatscr_inited) {
        cheatscr_initScreen();
    }
    if (gActiveEdges & (BUTTON_START | BUTTON_B)) {
        AudPlayEffect((gActiveEdges & BUTTON_START) ? SND_MENU_START : SND_MENU_BACK, 0, 1, 0);
        gCheatMenuExit = 2;
    } else if (gActiveAutoEdges & (BUTTON_UP | BUTTON_DOWN)) {
        if (gActiveAutoEdges & BUTTON_UP) {
            AudPlayEffect(SND_MENU_UP, 0, 1, 0);
            if (--cheatscr_gSelectedOption < 0) {
                cheatscr_gSelectedOption = CHEAT_COUNT - 1;
            }
        } else {
            AudPlayEffect(SND_MENU_DOWN, 0, 1, 0);
            if (++cheatscr_gSelectedOption >= CHEAT_COUNT) {
                cheatscr_gSelectedOption = 0;
            }
        }
        InitEgg(cheatscr_gSelectedOption);
    }
    if ((gActiveEdges & (BUTTON_LEFT | BUTTON_RIGHT)) && PC_CHEAT_ACTIVE(gCheatActive[cheatscr_gSelectedOption])) {
        s32 dir = (gActiveEdges & BUTTON_LEFT) ? -1 : 1;

        PlayLRSound(gActiveEdges);
        switch (cheatscr_gSelectedOption) {
        case 0:
            gNoCarCollisions ^= 1;
            gCheatState[0] = gNoCarCollisions;
            break;
        case 1:
            gMineCones ^= 1;
            gCheatState[1] = gMineCones;
            break;
        case 2:
            gCarMines ^= 1;
            gCheatState[2] = gCarMines;
            break;
        case 3:
            gCheatState[3] ^= 1;
            break;
        case 4:
            gNoAutoAbort ^= 1;
            gCheatState[4] = gNoAutoAbort;
            break;
        case 5:
            gSuperSpeed ^= 1;
            gCheatState[5] = gSuperSpeed;
            break;
        case 6:
            gInvincible ^= 1;
            gCheatState[6] = gInvincible;
            break;
        case 7:
            gInvisibleCar ^= 1;
            gCheatState[7] = gInvisibleCar;
            break;
        case 8:
            gInvisibleTrack ^= 1;
            gCheatState[8] = gInvisibleTrack;
            break;
        case 9:
            gNoBrakes ^= 1;
            gCheatState[9] = gNoBrakes;
            break;
        case 10:
            gSpecialTires = !gSpecialTires ? 2 : 0;
            gCheatState[10] = gSpecialTires == 2;
            break;
        case 11:
            gMassiveMass += dir;
            if (gMassiveMass > 2) {
                gMassiveMass = 0;
            } else if (gMassiveMass < 0) {
                gMassiveMass = 2;
            }
            gCheatState[11] = gMassiveMass;
            break;
        case 12:
            gSuicideMode += dir;
            if (gSuicideMode > 2) {
                gSuicideMode = 0;
            } else if (gSuicideMode < 0) {
                gSuicideMode = 2;
            }
            gCheatState[12] = gSuicideMode;
            break;
        case 13:
            gCheatState[13] ^= 1;
            break;
        case 14:
            gCheatState[14] ^= 1;
            break;
        case 15:
            gCheatState[15] ^= 1;
            break;
        case 16:
            gCheatState[16] ^= 1;
            break;
        case 17:
            gResurrectInPlace ^= 1;
            gCheatState[17] = gResurrectInPlace;
            break;
        case 18:
            gFrameScale += dir;
            if (gFrameScale > 3) {
                gFrameScale = 0;
            } else if (gFrameScale < 0) {
                gFrameScale = 3;
            }
            gCheatState[18] = gFrameScale;
            break;
        case 19:
            gEnableTireScaling ^= 1;
            gCheatState[19] = gEnableTireScaling;
            break;
        case CHEAT_FOG_COLOR:
            if (gFogColor < FOG_COLOR_BASE + 1) {
                gFogColor = 0;
            } else {
                gFogColor -= FOG_COLOR_BASE;
            }
            gFogColor += dir;
            if (gFogColor > FOG_COLORS) {
                gFogColor = 0;
            } else if (gFogColor < 0) {
                gFogColor = FOG_COLORS;
            }
            gCheatState[CHEAT_FOG_COLOR] = gFogColor;
            if (gFogColor) {
                gFogColor += FOG_COLOR_BASE;
            }
            break;
        }
    }
    if (!gCheatActive[cheatscr_gSelectedOption]) {
        gCheatActive[cheatscr_gSelectedOption] = CheckForEgg();
    }
    if (gSoftReset) {
        cheatscr_uninitScreen();
        setGameState(GS_TITLE);
    } else if (gCheatMenuExit) {
        cheatscr_uninitScreen();
        setGameState(GS_START);
    }
}

/* Set the screen up: gCheatState from the game's flags (the fog colour's raw value, clamped to 0-8, so a colour
 * set shows as none until changed), the text blit, the first row selected and its code watched. Rows 3, 13-16 keep
 * their gCheatState as it was. */
void cheatscr_initScreen(void)
{
    gCheatState[0] = gNoCarCollisions;
    gCheatState[1] = gMineCones;
    gCheatState[2] = gCarMines;
    gCheatState[4] = gNoAutoAbort;
    gCheatState[5] = gSuperSpeed;
    gCheatState[6] = gInvincible;
    gCheatState[7] = gInvisibleCar;
    gCheatState[8] = gInvisibleTrack;
    gCheatState[9] = gNoBrakes;
    gCheatState[10] = gSpecialTires == 2;
    gCheatState[11] = gMassiveMass;
    gCheatState[12] = gSuicideMode;
    gCheatState[17] = gResurrectInPlace;
    gCheatState[18] = gFrameScale;
    gCheatState[19] = gEnableTireScaling;
    gCheatState[CHEAT_FOG_COLOR] = gFogColor;
    if (gCheatState[CHEAT_FOG_COLOR] > FOG_COLORS) {
        gCheatState[CHEAT_FOG_COLOR] = 0;
    } else if (gCheatState[CHEAT_FOG_COLOR] < 0) {
        gCheatState[CHEAT_FOG_COLOR] = FOG_COLORS;
    }
    createBG();
    cheatscr_blit = NewMultiBlit(0, 0, cheatscr_multiblit, 1);
    cheatscr_gSelectedOption = 0;
    gCheatMenuExit = 0;
    InitEgg(cheatscr_gSelectedOption);
    cheatscr_inited = 1;
}

/* Take the screen down: its text blit and the background. */
void cheatscr_uninitScreen(void)
{
    if (cheatscr_blit) {
        RemoveBlit(cheatscr_blit);
        cheatscr_blit = 0;
    }
    removeBG();
    cheatscr_inited = 0;
}

#define TEXT_CHEAT_TITLE 54         /* gText.single[]: the screen's title */

/* The screen's text blit callback (MAT3 animText): the title, then 13 rows centred on the selected cheat, each its
 * name and its value (the frame scale as a number, else the cheat's own value names: gText's second table, the
 * cheat's name followed by its values). Always 1. */
s32 cheatscr_animText(Blit *blit)
{
    s32 x;
    s32 i;
    s32 y;
    s32 cheat;
    u8 *str;
    u8 str1[4];
    u8 *name;

    HAL_SetTextDepth(0.0f);
    HAL_SetFont(10);
    TB_SetTextColor(12);
    TB_DrawText(TB_CenterText(gText.single[TEXT_CHEAT_TITLE], 151), (18 - HAL_GetFontAscent()) / 2 + 6, gText.single[TEXT_CHEAT_TITLE]);
    HAL_SetFont(12);
    x = 154;
    y = 47;
    for (i = 0; i < ROWS_SHOWN; i++) {
        cheat = i + cheatscr_gSelectedOption - 6;
        if (cheat < 0) {
            cheat += CHEAT_COUNT;
        } else if (cheat >= CHEAT_COUNT) {
            cheat -= CHEAT_COUNT;
        }
        if (cheat == CHEAT_FRAME_SCALE) {
            TB_Sprintf(str1, cheatscr_number_format, gFrameScale);
            str = str1;
        } else {
            str = gText.tab2b[(u32)gText.tab2a[*gText.tab2 + cheat] + gCheatState[cheat] + 1];
        }
        name = gText.tab2b[gText.tab2a[*gText.tab2 + cheat]];
        cheatscr_printOption(x, y, cheat, name, str);
        y += TB_GetFontLineHeight();
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}

/* One row (MAT3 printOption): the name right-aligned before x and the value after it, the selected row flashing
 * in the larger font, a locked cheat's value grey. */
void cheatscr_printOption(s32 x, s32 y, s32 item, u8 *name, u8 *value)
{
    HAL_SetFont(cheatscr_gSelectedOption == item ? 11 : 12);
    TB_SetTextColor(cheatscr_gSelectedOption == item ? kTB_FlashText : kTB_WhiteText);
    TB_DrawText(TB_RightText(name, x - 4), y - (cheatscr_gSelectedOption == item ? 1 : 0), name);
    HAL_SetFont((PC_CHEAT_ACTIVE(gCheatActive[item]) && cheatscr_gSelectedOption == item) ? 11 : 12);
    TB_SetTextColor(PC_CHEAT_ACTIVE(gCheatActive[item]) ? (cheatscr_gSelectedOption == item ? 11 : 1) : 4);
    TB_DrawText(x + 12, y - ((PC_CHEAT_ACTIVE(gCheatActive[item]) && cheatscr_gSelectedOption == item) ? 1 : 0), value);
}
