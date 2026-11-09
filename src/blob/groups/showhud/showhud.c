/* ShowHUD: make the race's HUD (MAT3 hud.c). */
#include "game/game.h"

typedef struct Blit Voice;
typedef struct Pane Pane;
typedef s32 (*PaneAnimation)(Voice *);
typedef struct MultiBlit {
    const char *texname; s16 dulx,duly,width,height,top,bot,left,right;
    u32 zdepth,alpha; s32 (*animfunc)(Voice *); u32 animid;
} MultiBlit;


/* PC: the race set with a minimap dot and a radar blip for each car past the N64's 8 (docs/port/MORECARS.md 7). The
 * set (RaceMultiBlit, ROM data) has a dot for each of the HUD order's first 8 entries (AnimateDot, animDot: AnimID
 * the entry) and, for each of the 4 viewers, a blip for each of car slots 0-7 (AnimateTraffic, animRadarBlip: AnimID
 * bits 8-11 the viewer, 0-3 the slot), each run of them ending at 7. With more than 8 cars the humans, last in the
 * HUD order, had no dot, and slots 8-11 no blip. So a race of more than 8 cars gets a copy of the set, made at run
 * time, with each run continued after its 7 by copies of that entry for 8 to the race's cars less one; a race of 8 or
 * fewer gets the set itself, as the N64 does. */
#define PC_HUD_RACE_ROM 89
#define PC_HUD_RACE_RUNS 8   /* room for 8 runs to grow: the set has 5 */
#define PC_HUD_RACE_MAX (PC_HUD_RACE_ROM + PC_HUD_RACE_RUNS * (PC_MAX_LINKS - 8))
static MultiBlit pc_hud_race[PC_HUD_RACE_MAX];
static s16 pc_hud_race_count;

/* Builds pc_hud_race: the race set with each run of dots and blips continued past car 7 to the race's cars. */
static void pc_hud_race_build(void)
{
    s32 cars = PC_RACE_CARS, i, k, n = 0;

    for (i = 0; i < PC_HUD_RACE_ROM; i++) {
        const MultiBlit *e = &RaceMultiBlit[i];
        pc_hud_race[n++] = *e;
        if ((e->animfunc == animDot || e->animfunc == animRadarBlip) && (e->animid & 0xF) == 7) {
            for (k = 8; k < cars && n < PC_HUD_RACE_MAX - (PC_HUD_RACE_ROM - 1 - i); k++) {
                pc_hud_race[n] = *e;
                pc_hud_race[n].animid = (e->animid & ~0xFu) | (u32)k;
                n++;
            }
        }
    }
    pc_hud_race_count = n;
}

/* The race set for this race: RaceMultiBlit itself for 8 cars or fewer, else the built copy. */
static const MultiBlit *pc_hud_race_set(void)
{
    if (PC_RACE_CARS <= 8) {
        return RaceMultiBlit;
    }
    pc_hud_race_build();
    return pc_hud_race;
}

/* The number of entries of pc_hud_race_set's set. */
static s16 pc_hud_race_n(void)
{
    if (PC_RACE_CARS <= 8) {
        return PC_HUD_RACE_ROM;
    }
    pc_hud_race_build();
    return pc_hud_race_count;
}
#define PC_HUD_RACE_SET pc_hud_race_set()
#define PC_HUD_RACE_N pc_hud_race_n()

/* Makes the race's HUD once (nothing when HudBlit1 exists): the multiblit of the race HUD (speedometer, tachometer,
 * gear, odometer, place, laps, clocks, map with a dot per car, radar with a blip per car ...), the split screen's or
 * the overlays' set by the number of windows, the result panes and the coin counters' objects; then in battle and
 * stunt races the extras (ShowHUDExtra). */
void ShowHUD(void) {
    s32 created = 0;
    s32 x;
    s32 y;
    static f32 delay;
    static f32 nextSwitch;
    if (HudBlit1 == 0) {
        s32 i; /* Reconstruction-only loop index; absent from original named-local list. */
        for (i = 0; i < 4; i++) {
            gCheckPtTimeout[i] = 0.0f;
            gLapTime[i] = 0.0f;
            gElapsedTime[i] = 0.0f;
            gPlayerCoin[i].slot[0].id = -1;
            gPlayerCoin[i].slot[1].id = -1;
        }
        if (gGameState & GS_HIGH_SCORE) {
            HudBlit1 = NewMultiBlit(0, 0, OverlayMultiBlit, 1);
        } else if (gGameState & GS_DEMO) {
            HudBlit1 = NewMultiBlit(0, 0, OverlayMultiBlit, 1);
        } else {
            if (expansion_pak_present || gTrackNum >= 6) {
                if (gNumWindows == 2) {
                    multiPlayerBlit = NewMultiBlit(0, 0, TwoPlayerMultiBlit, 1);
                } else if (gNumWindows == 3) {
                    multiPlayerBlit = NewMultiBlit(0, 0, ThreePlayerMultiBlit, 1);
                }
            }
            if (gRace == kBattle || gRace == kStunt) {
                ShowHUDExtra();
            } else {
                HudBlit2 = NewMultiBlit(0, 0, PC_HUD_RACE_SET, PC_HUD_RACE_N);
            }
            HudBlit1 = NewMultiBlit(0, 0, HudMultiBlit, 8);
        }
        markers_valid = 0;
        created = 1;
    }
    if ((gGameState & GS_DEMO) && gShowCredits) {
        if (created) {
            gCredit = -1;
            SetCountdownTimer(90.0f);
            delay = GetCountdownTimeRemaining() / 13.0f;
            nextSwitch = gSeconds;
        }
        if (nextSwitch <= gSeconds) {
            nextSwitch += delay;
            gCredit++;
            gCorner = Random(4.0f);
            HAL_SetFont(11);
            gCreditWidth = HAL_GetTextWidth(gText.taba[gText.tab[0] + gCredit], -1);
            for (gNumCreditNames = 0; cStrCreditNames[gCredit].names[gNumCreditNames]; gNumCreditNames++) {
                if (gCreditWidth < HAL_GetTextWidth(cStrCreditNames[gCredit].names[gNumCreditNames], -1)) {
                    gCreditWidth = HAL_GetTextWidth(cStrCreditNames[gCredit].names[gNumCreditNames], -1);
                }
            }
            switch (gCorner) {
            case 0:
                x = 20;
                y = 20;
                break;
            case 1:
                x = 280 - gCreditWidth;
                y = 20;
                break;
            case 2:
                x = 20;
                y = 145 - gNumCreditNames * 10;
                break;
            default:
                x = 280 - gCreditWidth;
                y = 145 - gNumCreditNames * 10;
                break;
            }
            RemovePanes(0);
            InitPane(x, y, x + gCreditWidth + 20, gNumCreditNames * 10 + y + 32, 192, 0, 0, 0);
        }
    }
}

 
