/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * ShowHUD (0x800F0100, 1,396 B; historical label) = MAT3 ShowHUD@hud.c (GC 0x8001C980, 904 B;
 * reference/repos/mat3/decomp/src/GameSource/src/hud.c). Creates the HUD blit sets on first call (clears the
 * per-player checkpoint/lap/elapsed timers and coin records, then NewMultiBlit = NewMultiBlit: overlay set for
 * attract/credits, 2/3-player divider sets, ShowHUDExtra (image B 0x803925D0) for battle/stunt or the race set,
 * then the HUD set), and in attract mode with credits on cycles the credit panes (SetCountdownTimer =
 * SetCountdownTimer, GetCountdownTimeRemaining, Random = Random inlined, HAL_SetFont,
 * HAL_GetTextWidth, RemovePanes, InitPane).
 * N64 constants differ from the GC (race/HUD blit counts 89/8, credit delay /13, bottom row 145; the 2/3-player
 * sets only when expansion_pak_present || gTrackNum >= 6).
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * function-local statics delay/nextSwitch, credit-name for-loop, x-before-y corner order), unchanged except
 * one lever: HAL_SetFont is inlined (as in retail: osRecvMesg + HAL_SetFontInternal(s2=11) + osSendMesg)
 * through non-static copies HAL_SetFont_ShowHUD/_GetSemaphore_ShowHUD/_ReleaseSemaphore_ShowHUD defined after the
 * caller (not on keep, the locked HAL_SetFont is kept and stays a jal in the unit, 335/349 words off).
 * Shaping (disclosed): _ReleaseSemaphore_ShowHUD and HAL_SetFont_ShowHUD are written on ONE source line. as1 breaks
 * ready-list ties by source line; in the group compile the inlined bodies keep their own .loc lines, and retail's
 * order (addiu a0 / move s0,v0 / move a1 before osSendMesg) needs the osSendMesg and `old = HAL_SetFontInternal()`
 * statements on the same line (the whole-program unit gives every inlined instruction the call-site line, where
 * the placement does not matter: unit EQUAL either way; group 2 words swapped otherwise).
 * gSeconds is `extern volatile float` as in MAT3 rush_protos.h (gSeconds).
 * The block-local loop index `i` is a reconstruction (absent from the DWARF local list of ShowHUD).
 * MATCH: whole-program unit EQUAL (the whole-program unit --tag); group showhud (the scorer) MATCH.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct Blit Voice;
typedef struct Pane Pane;
typedef s32 (*PaneAnimation)(Voice *);
typedef struct MultiBlit {
    const char *texname; s16 dulx,duly,width,height,top,bot,left,right;
    u32 zdepth,alpha; s32 (*animfunc)(Voice *); u32 animid;
} MultiBlit;
typedef struct RecordSlot { s16 id; u8 pad02[50]; } RecordSlot;   /* 52 bytes */
typedef struct RecordRow { RecordSlot a, b; } RecordRow;              /* 104 bytes */
/* Complete native resource-table layout, shared with its actual loader. */
/* PC: gText's pointer sections are native char * tables (port/assets/text.c) */
typedef struct ModelTables {
    u16 count0; char **ptrs0; u16 count1,count2; u16 *data1; char **ptrs2;
    u16 count3,count4,count5; u16 *data3,*data4; char **ptrs5;
} ModelTables;

extern Voice *HudBlit1;
extern Voice *HudBlit2;
extern Voice *multiPlayerBlit;
extern RecordRow gPlayerCoin[];
extern f32 gCheckPtTimeout[4], gLapTime[4], gElapsedTime[4];
extern u32 gGameState;
extern s8 expansion_pak_present;
extern s8 gTrackNum;
extern s16 gNumWindows;
extern s32 gRace;
extern MultiBlit OverlayMultiBlit[], TwoPlayerMultiBlit[], ThreePlayerMultiBlit[], RaceMultiBlit[], HudMultiBlit[];
extern s16 markers_valid;
extern s32 gShowCredits;
extern s32 gCredit;
extern volatile f32 gSeconds;
extern s32 gCorner;
extern s32 gCreditWidth;
extern s32 gNumCreditNames;
extern char *cStrCreditNames[][11];
extern ModelTables gText;

Voice *NewMultiBlit(s16, s16, const MultiBlit *, s16);
Pane *InitPane(s32,s32,s32,s32,s32,PaneAnimation,u32,s32);
void RemovePanes(s32 arg0);
void SetCountdownTimer(f32);
f32 GetCountdownTimeRemaining(void);
void ShowHUDExtra(void);
typedef struct { u32 opaque[6]; } OSMesgQueue;
extern OSMesgQueue text_lock_queue;
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 fontID);
void _GetSemaphore_ShowHUD(void);
void _ReleaseSemaphore_ShowHUD(void);
s32 HAL_SetFont_ShowHUD(s32 fontID);
extern u32 HAL_GetTextWidth(void *, s16);

extern f32 Random(f32);

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
s32 animDot(Voice *);   /* AnimateDot */
s32 animRadarBlip(Voice *);   /* AnimateTraffic */

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

static const MultiBlit *pc_hud_race_set(void)
{
    if (PC_RACE_CARS <= 8) {
        return RaceMultiBlit;
    }
    pc_hud_race_build();
    return pc_hud_race;
}

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
            gPlayerCoin[i].a.id = -1;
            gPlayerCoin[i].b.id = -1;
        }
        if (gGameState & 0x02000000) {
            HudBlit1 = NewMultiBlit(0, 0, OverlayMultiBlit, 1);
        } else if (gGameState & 8) {
            HudBlit1 = NewMultiBlit(0, 0, OverlayMultiBlit, 1);
        } else {
            if (expansion_pak_present || gTrackNum >= 6) {
                if (gNumWindows == 2) {
                    multiPlayerBlit = NewMultiBlit(0, 0, TwoPlayerMultiBlit, 1);
                } else if (gNumWindows == 3) {
                    multiPlayerBlit = NewMultiBlit(0, 0, ThreePlayerMultiBlit, 1);
                }
            }
            if (gRace == 6 || gRace == 4) {
                ShowHUDExtra();
            } else {
                HudBlit2 = NewMultiBlit(0, 0, PC_HUD_RACE_SET, PC_HUD_RACE_N);
            }
            HudBlit1 = NewMultiBlit(0, 0, HudMultiBlit, 8);
        }
        markers_valid = 0;
        created = 1;
    }
    if ((gGameState & 8) && gShowCredits) {
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
            HAL_SetFont_ShowHUD(11);
            gCreditWidth = HAL_GetTextWidth((void *)gText.ptrs2[gText.data1[0] + gCredit], -1);
            for (gNumCreditNames = 0; cStrCreditNames[gCredit][gNumCreditNames]; gNumCreditNames++) {
                if (gCreditWidth < HAL_GetTextWidth(cStrCreditNames[gCredit][gNumCreditNames], -1)) {
                    gCreditWidth = HAL_GetTextWidth(cStrCreditNames[gCredit][gNumCreditNames], -1);
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

void _GetSemaphore_ShowHUD(void) { osRecvMesg(&text_lock_queue, (void **)0, 1); }
void _ReleaseSemaphore_ShowHUD(void) { osSendMesg(&text_lock_queue, (void *)0, 0); } s32 HAL_SetFont_ShowHUD(s32 fontID) { s32 old; _GetSemaphore_ShowHUD(); old = HAL_SetFontInternal(fontID); _ReleaseSemaphore_ShowHUD(); return old; }
