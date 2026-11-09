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

extern void particle_lifetime_set(void);
extern Blit *NewMultiBlit(s16, s16, const MultiBlit *, s16);
extern void RemoveBlit(Blit *);

/* PC: game.cheats_unlock (port/config/cheats.c, docs/port/FRONTEND.md "The cheat screen in the main menu"): every
   cheat is available while it is on, its code not needed: the tests of gCheatActive[cheat] that let LEFT and RIGHT
   switch it and draw it bright take it as set. The flags aren't written; off, the N64's. */
int pc_cheat_active(int active);
#define PC_CHEAT_ACTIVE(a) pc_cheat_active(a)

void updateCheatScreen(void)
{
    if (!cheatscr_inited) {
        cheatscr_initScreen();
    }
    if (gActiveEdges & 5) {
        AudPlayEffect((gActiveEdges & 1) ? 46 : 38, 0, 1, 0);
        gCheatMenuExit = 2;
    } else if (gActiveAutoEdges & 0xc00) {
        if (gActiveAutoEdges & 0x400) {
            AudPlayEffect(40, 0, 1, 0);
            if (--cheatscr_gSelectedOption < 0) {
                cheatscr_gSelectedOption = 20;
            }
        } else {
            AudPlayEffect(39, 0, 1, 0);
            if (++cheatscr_gSelectedOption >= 21) {
                cheatscr_gSelectedOption = 0;
            }
        }
        InitEgg(cheatscr_gSelectedOption);
    }
    if ((gActiveEdges & 0x3000) && PC_CHEAT_ACTIVE(gCheatActive[cheatscr_gSelectedOption])) {
        s32 dir = (gActiveEdges & 0x1000) ? -1 : 1;

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
        case 20:
            if (gFogColor < 19) {
                gFogColor = 0;
            } else {
                gFogColor -= 18;
            }
            gFogColor += dir;
            if (gFogColor > 8) {
                gFogColor = 0;
            } else if (gFogColor < 0) {
                gFogColor = 8;
            }
            gCheatState[20] = gFogColor;
            if (gFogColor) {
                gFogColor += 18;
            }
            break;
        }
    }
    if (!gCheatActive[cheatscr_gSelectedOption]) {
        gCheatActive[cheatscr_gSelectedOption] = CheckForEgg();
    }
    if (gSoftReset) {
        cheatscr_uninitScreen();
        setGameState(4);
    } else if (gCheatMenuExit) {
        cheatscr_uninitScreen();
        setGameState(16);
    }
}

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
    gCheatState[20] = gFogColor;
    if (gCheatState[20] > 8) {
        gCheatState[20] = 0;
    } else if (gCheatState[20] < 0) {
        gCheatState[20] = 8;
    }
    particle_lifetime_set();
    cheatscr_blit = NewMultiBlit(0, 0, cheatscr_multiblit, 1);
    cheatscr_gSelectedOption = 0;
    gCheatMenuExit = 0;
    InitEgg(cheatscr_gSelectedOption);
    cheatscr_inited = 1;
}

void cheatscr_uninitScreen(void)
{
    if (cheatscr_blit) {
        RemoveBlit(cheatscr_blit);
        cheatscr_blit = 0;
    }
    removeBG();
    cheatscr_inited = 0;
}

/* cheatscr_animText = animText@cheatscr.c (GC 0x80067E68), the multiblit callback; printOption@cheatscr.c
 * (GC 0x80067D20) is inlined into it and its deleted body is the stub cheatscr_printOption (non-static, off keep).
 * MAT3 port with the Dreamcast unit's `int x = 154` (dreamcast src/cheatscr.c animText) and a `name` local for
 * the label: with both named locals the retail frame (136, str1 at +112) comes out with no unused homes.
 * gText gText (tab2 +0x1C, tab2a +0x20, tab2b +0x24, single +4); HAL_SetTextDepth HAL_SetTextDepth,
 * HAL_SetFont HAL_SetFont, TB_SetTextColor TB_SetTextColor, TB_CenterText TB_CenterText,
 * TB_RightText TB_RightText, HAL_GetFontAscent HAL_GetFontAscent, TB_DrawText TB_DrawText,
 * TB_Sprintf TB_Sprintf ("%d" = cheatscr_number_format), TB_GetFontLineHeight TB_GetFontLineHeight.
 * N64: 21 options; only cheat 18 (gFrameScale gFrameScale) prints a number (GC also 15, gRandomWeapons).
 * Shaping, measured: the value index is written `(u32)tab2a[...] + gCheatState[cheat] + 1` (the unsigned label
 * offset first, as found): MAT3's `state + 1 + tab2a[.]`
 * emits 190 instructions (the label*4 is not shared between the two branches). */

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
    TB_DrawText(TB_CenterText(gText.single[54], 151), (18 - HAL_GetFontAscent()) / 2 + 6, gText.single[54]);
    HAL_SetFont(12);
    x = 154;
    y = 47;
    for (i = 0; i < 13; i++) {
        cheat = i + cheatscr_gSelectedOption - 6;
        if (cheat < 0) {
            cheat += 21;
        } else if (cheat >= 21) {
            cheat -= 21;
        }
        if (cheat == 18) {
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

void cheatscr_printOption(s32 x, s32 y, s32 item, u8 *name, u8 *value)
{
    HAL_SetFont(cheatscr_gSelectedOption == item ? 11 : 12);
    TB_SetTextColor(cheatscr_gSelectedOption == item ? 22 : 1);
    TB_DrawText(TB_RightText(name, x - 4), y - (cheatscr_gSelectedOption == item ? 1 : 0), name);
    HAL_SetFont((PC_CHEAT_ACTIVE(gCheatActive[item]) && cheatscr_gSelectedOption == item) ? 11 : 12);
    TB_SetTextColor(PC_CHEAT_ACTIVE(gCheatActive[item]) ? (cheatscr_gSelectedOption == item ? 11 : 1) : 4);
    TB_DrawText(x + 12, y - ((PC_CHEAT_ACTIVE(gCheatActive[item]) && cheatscr_gSelectedOption == item) ? 1 : 0), value);
}
