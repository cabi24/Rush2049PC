/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image A start screen (title "press start" menu): updateStartScreen (updateStartScreen, kept root) and its
 * IPA-internal updateButtons (startscr_updateButtons). Donor: MAT3 GameSource startscr.c, updateStartScreen (GC 0x8008624c)
 * and static updateButtons (GC 0x800871e4); direct typed port, N64 offsets/addresses from the retail words.
 *
 * N64 differences from MAT3 (all read from the retail code): gOptions[14] <= 12 (GC 18); AudPlaySong(6,1) (GC 19);
 * PlayLRSound() runs before the `gStartMenuSelect == 0` test; exit case 7 -> 0x10000000 (GC has no case 7);
 * lower angle limit in updateButtons is 0.0f (GC 0.08726646f); the gNumPlayers clamp is
 * `if (n > max) { if (max < 1) n = 1; else n = max; }`; the 17th button x is `gNumPlayers * 22.5f` (ugen operand order).
 * gFrameSeconds is volatile (MAT3 declares it volatile) and gNumModels is volatile u8 (every use is lui/addiu/lbu 0(reg)).
 * uninitScreen and uninitButtons are inlined statics (inlined twice into the root). startscr_optionValid (optionValid) is the
 * installed body of group runtime_a_option_eligibility, present only as IPA context (without it the
 * root keeps &gStartMenuSelect in s0 instead of a2).
 *
 * supersession: adds initScreen (startscr_initScreen, MAT3 startscr.c initScreen GC 0x800865f8) with
 * initButtons (GC 0x80086da4) inlined as non-kept startscr_initButtons (retail deleted stub 0x8038FFD0) and eggs.c
 * InitEgg(23) inlined (non-kept `InitEgg`; store order gEgg, gNextTimeOut, gEggIdx; its `= 0` spelling keeps it off
 * initButtons' 0.0f web). N64 differences from MAT3: gOptions[14] <= 12, AudPlaySong(6, 1), texture "BUTTON_TITLE"
 * for button 11, the two 55.2f spellings (55.2f / 55.20f: retail has two rodata words), no MBOX_HideObject of
 * buttons 14/15, volatile gNumModels. Functions are in MAT3 file order: as1 breaks scheduling ties by source line,
 * so the inlined initButtons must follow initScreen (with initButtons first, `addiu s2,sp,76` lost a tie: 7 words).
 * The bodies of updateStartScreen, updateButtons, optionValid, uninitScreen and uninitButtons are unchanged.
 *
 * : uninitButtons and uninitScreen are non-static functions named after their retail deleted stubs
 * startscr_uninitButtons / startscr_uninitScreen (post-order of updateStartScreen: CheckForEgg F648, then uninitScreen's subtree
 * uninitButtons F734, uninitScreen F73C, then optionValid F744, updateButtons F79C, InitEgg = locked single FF90,
 * initButtons FFD0, initScreen FFD8). Off keep, so umerge still inlines them; as statics they had no symbol.
 *
 * Frame shaping, disclosed (rule): updateButtons has retail frame 96. Without the three unused f32 locals
 * below the frame is 88 and the locals sit at sp+56/66/72 instead of retail's sp+52/62/68. Udef is 32 bytes with
 * the MAT3 locals (buttonIdx, prevPos[3], tex, texIdx, buttonY, selectedY) and 44 with three more 4-byte slots
 * (frame = align8(52 + Udef)). The three unused locals reproduce this exactly; their original use is unknown.
 */
typedef signed char s8;typedef unsigned char u8;
typedef short s16;typedef unsigned short u16;typedef int s32;typedef unsigned int u32;typedef float f32;
typedef struct Blit Blit;
typedef struct ButtonInfo { s32 buttonType; s32 objIdx; f32 angle; f32 uvs[4][3]; } ButtonInfo;
typedef union tRGBA { u8 c[4]; u32 w; } tRGBA;
typedef struct MultiBlit MultiBlit;
typedef struct Egg Egg;
/* gcc rejects arrays of an incomplete type, which IDO accepts. Layouts as in src/blob/NewMultiBlit.c (MultiBlit)
   and src/ovl_a/groups/cheat_screen/eggs.c (Egg). */
struct MultiBlit { const char *texname; s16 dulx, duly, width, height, top, bot, left, right; u32 zdepth, alpha;
                   s32 (*animfunc)(Blit *); u32 animid; };
struct Egg { u8 size; u8 pad[3]; u32 levels[20]; u32 edges[20]; };
typedef struct Controller { s8 attached; u8 rest[15]; } Controller;

extern volatile f32 gFrameSeconds;
extern f32 unitmatrix[3][3];      /* unitmatrix */
extern volatile u8 gNumModels;
extern s8 gOptions[];           /* gOptions */
extern s8 gGameStarted;
extern s8 gSoftReset;
extern u32 gGameState, gSkipToGameState;/* gGameState, gSkipToGameState */
extern u32 gActiveEdges;
extern s16 gNumPlayers;
extern Controller gControllerData[4];  /* gControllerData */
extern s8 cheat_menu_unlocked;             /* gCheatActive[23] */
extern s8 startscr_validOption[];           /* validOption */
extern u32 gActiveLevels;
extern ButtonInfo startscr_buttonInfo[18]; /* buttonInfo */
extern s8 startscr_inited;             /* inited */
extern Blit *startscr_blit;          /* blit */
extern s32 startscr_buttonsInited;            /* buttonsInited */
extern s32 startscr_buttonsJustInited;            /* buttonsJustInited */
extern f32 startscr_gButtonScale;            /* gButtonScale */
extern f32 startscr_gButtonSpace;            /* gButtonSpace */
extern f32 startscr_buttonX;            /* buttonX */
extern f32 startscr_maxY;            /* maxY */
extern f32 startscr_buttonZ;            /* buttonZ */
extern f32 startscr_cursorZ;            /* cursorZ */
extern s32 gStartMenuSelect;
extern s32 gMaxPlayers;
extern s32 exitStartScreen;
extern f32 startscr_cursorSpeed;            /* cursorSpeed */
extern f32 playerCursorSpeed;
extern char *startscr_buttonNames[];        /* buttonNames */
extern MultiBlit startscr_mblit[];    /* mblit */
extern Egg start_screen_egg[];          /* gEggs + 23 entries is the start-screen egg */
extern Egg *gEgg;
extern f32 gNextTimeOut;
extern s32 gEggIdx;

extern f32 fabsf(f32);
#pragma intrinsic (fabsf)
extern void *memcpy(void *, const void *, u32);
extern void particle_lifetime_set(void);      /* createBG */
extern s32 MBOX_FindObject_WORK(char *, s8, s8, s8);
extern s32 MBOX_NewObject(s32, f32 (*)[3], s32, s32);
extern void ZOID_SetColor(s16, tRGBA *);
extern void ZeroCameras(void);
extern Blit *NewMultiBlit(s16, s16, MultiBlit *, s16);
extern void ClearRushControls(void);
extern s8 CheckForEgg(void);
extern void setGameState(s32);
extern void AudSetMusicVol(f32, f32);
extern void AudPlaySong(s32, s32);
extern void PlayStartABSound(u32);
extern void PlayLRSound(u32);
extern void PlayUDSound(u32);
extern void RemoveBlit(Blit *);
extern void removeBG(void);
extern void ZOID_RemoveObject(s16);
extern void AudKillSong(void);
extern void AudStopAllEffects(void);
extern void ExitSystem(void);
extern void MBOX_HideObject(s32, s32, s32);
extern void MBOX_ShowObject(s32, s32, s32);
extern void *MBOX_FindTexture_WORK(char *, u16 *, s8, s8, s32);
extern void ZOID_SetTexture(s16, void *, s32);
extern void ScaleUV(f32 *, f32 *, f32);
extern void WPitchUV(f32, f32 *);
extern void WRollUV(f32, f32 *);
extern void ScaleUVXYZ(f32 *, f32 *, f32, f32, f32);

void startscr_initScreen(void);
s8 startscr_optionValid(s32);
void startscr_initButtons(void);
void startscr_uninitButtons(void); /* uninitButtons@startscr.c: deleted stub 0x8038F734 */
void startscr_uninitScreen(void); /* uninitScreen@startscr.c: deleted stub 0x8038F73C */
void startscr_updateButtons(void);
/* startscr_InitEgg is this file's InitEgg (inlined on the N64): it points at the main menu's code, start_screen_egg;
   eggs.c's InitEgg at cheat `egg` of gEggs. Under one name the PC linked eggs.c's for both, so InitEgg(23)
   pointed past the table and the cheat screen could never be opened (docs/port/QA.md). */
/* PC: game.pc_setup (port/config/start_menu.c, docs/port/FRONTEND.md "PC SETUP"): option slot 8, never valid on the
   N64, is PC SETUP while it is on, A on it opens the settings overlay over the menu; game.cheats_menu: slot 6 valid. */
int pc_start_valid(int index, int value);
int pc_start_exit(int exit, int select);
#define PC_START_VALID(i, v) pc_start_valid((i), (v))
#define PC_START_EXIT(e) ((e) = pc_start_exit((e), gStartMenuSelect))
void startscr_InitEgg(s32);
void updateStartScreen(void);








void startscr_InitEgg(s32 egg)
{
    gEgg = &start_screen_egg[egg - 23];
    gNextTimeOut = 0;
    gEggIdx = 0;
}

void updateStartScreen(void)
{
    s32 ctrlIdx;
    if (!gSoftReset)
        gGameStarted = 1;
    if (gSkipToGameState != 1) {
        if (gSkipToGameState != gGameState) {
            if (!(gSkipToGameState & 0x7c0000) && gOptions[14] <= 12) {
                AudSetMusicVol(gOptions[12] / 10.0f, 0.05f);
                AudPlaySong(6, 1);
            }
            setGameState(32);
            return;
        }
        gSkipToGameState = 1;
    }
    if (!startscr_inited)
        startscr_initScreen();
    while (!startscr_optionValid(gStartMenuSelect)) {
        if (++gStartMenuSelect >= 10)
            gStartMenuSelect = 0;
    }
    gMaxPlayers = 0;
    for (ctrlIdx = 0; ctrlIdx < 4; ctrlIdx++) {
        if (gControllerData[ctrlIdx].attached != 1)
            break;
        gMaxPlayers++;
    }
    if (gNumPlayers > gMaxPlayers) {
        if (gMaxPlayers < 1)
            gNumPlayers = 1;
        else
            gNumPlayers = gMaxPlayers;
    }
    if (gActiveEdges & 1) {
        PlayStartABSound(gActiveEdges);
        gStartMenuSelect = 0;
        exitStartScreen = 1;
    } else if (gActiveEdges & 2) {
        PlayStartABSound(gActiveEdges);
        exitStartScreen = 1;
    } else if (gActiveEdges & 4) {
        PlayStartABSound(gActiveEdges);
        exitStartScreen = 2;
    } else if (gActiveEdges & 0x1000) {
        PlayLRSound(gActiveEdges);
        if (gStartMenuSelect == 0) {
            if (--gNumPlayers < 1)
                gNumPlayers = gMaxPlayers;
        }
    } else if (gActiveEdges & 0x2000) {
        PlayLRSound(gActiveEdges);
        if (gStartMenuSelect == 0) {
            if (++gNumPlayers > gMaxPlayers)
                gNumPlayers = 1;
        }
    } else if (gActiveEdges & 0x400) {
        PlayUDSound(gActiveEdges);
        do {
            if (--gStartMenuSelect < 0)
                gStartMenuSelect = 9;
        } while (!startscr_optionValid(gStartMenuSelect));
    } else if (gActiveEdges & 0x800) {
        PlayUDSound(gActiveEdges);
        do {
            if (++gStartMenuSelect >= 10)
                gStartMenuSelect = 0;
        } while (!startscr_optionValid(gStartMenuSelect));
    }
    startscr_updateButtons();
    if (gSoftReset) {
        startscr_uninitScreen();
        setGameState(4);
    } else if (PC_START_EXIT(exitStartScreen)) {
        startscr_uninitScreen();
        switch (exitStartScreen) {
            case 1:
                switch (gStartMenuSelect) {
                    case 0: setGameState(32); break;
                    case 1: setGameState(0x2000); break;
                    case 2: setGameState(0x200); break;
                    case 3: setGameState(0x800); break;
                    case 4: setGameState(0x1000); break;
                    case 6: setGameState(0x8000000); break;
                    case 7: setGameState(0x10000000); break;
                    case 9:
                        AudKillSong();
                        AudStopAllEffects();
                        ExitSystem();
                        break;
                }
                break;
            case 2:
                setGameState(4);
                break;
        }
        exitStartScreen = 0;
    }
    if (!cheat_menu_unlocked)
        cheat_menu_unlocked = CheckForEgg();
}

void startscr_initScreen(void)
{
    if (gOptions[14] <= 12) {
        AudSetMusicVol(gOptions[12] / 10.0f, 0.05f);
        AudPlaySong(6, 1);
    }
    particle_lifetime_set();
    startscr_initButtons();
    startscr_blit = NewMultiBlit(0, 0, startscr_mblit, 1);
    exitStartScreen = 0;
    ClearRushControls();
    if (!cheat_menu_unlocked)
        startscr_InitEgg(23);
    startscr_inited = 1;
}

void startscr_uninitScreen(void) /* uninitScreen (non-static, named after its retail stub) */
{
    if (startscr_blit) {
        RemoveBlit(startscr_blit);
        startscr_blit = 0;
    }
    removeBG();
    startscr_uninitButtons();
    startscr_inited = 0;
}

/* optionValid: installed body (group runtime_a_option_eligibility), IPA context only */
s8 startscr_optionValid(s32 index);

void startscr_initButtons(void) /* initButtons */
{
    tRGBA buttonColor = {255, 255, 255, 255};
    void *tex;
    u16 texIdx;
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < 18; buttonIdx++)
        if (startscr_buttonInfo[buttonIdx].objIdx == -1) {
            memcpy(startscr_buttonInfo[buttonIdx].uvs, unitmatrix, 48);
            startscr_buttonInfo[buttonIdx].objIdx = MBOX_NewObject(
                MBOX_FindObject_WORK(startscr_buttonNames[startscr_buttonInfo[buttonIdx].buttonType], 0, gNumModels - 1, 1),
                startscr_buttonInfo[buttonIdx].uvs, -1,
                (startscr_buttonInfo[buttonIdx].buttonType == 0 || startscr_buttonInfo[buttonIdx].buttonType == 1) ? 0x42000 : 0);
            ZOID_SetColor(startscr_buttonInfo[buttonIdx].objIdx, &buttonColor);
        }
    tex = MBOX_FindTexture_WORK("BUTTON_TITLE", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[11].objIdx, tex, -1);
    memcpy(startscr_buttonInfo[11].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[11].uvs);
    startscr_buttonInfo[11].uvs[3][0] = 0.0f;
    startscr_buttonInfo[11].uvs[3][1] = 115.0f;
    startscr_buttonInfo[11].uvs[3][2] = 200.0f;
    tex = MBOX_FindTexture_WORK("A1", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[12].objIdx, tex, -1);
    memcpy(startscr_buttonInfo[12].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[12].uvs);
    startscr_buttonInfo[12].uvs[3][0] = 75.1f;
    startscr_buttonInfo[12].uvs[3][1] = 55.2f;
    startscr_buttonInfo[12].uvs[3][2] = 100.0f;
    ScaleUV(startscr_buttonInfo[12].uvs[0], startscr_buttonInfo[12].uvs[0], 0.5f);
    memcpy(startscr_buttonInfo[13].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[13].uvs);
    WRollUV(3.1415927f, startscr_buttonInfo[13].uvs);
    startscr_buttonInfo[13].uvs[3][0] = 87.1f;
    startscr_buttonInfo[13].uvs[3][1] = 55.2f;
    startscr_buttonInfo[13].uvs[3][2] = 100.0f;
    tex = MBOX_FindTexture_WORK("B1", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[14].objIdx, tex, -1);
    memcpy(startscr_buttonInfo[14].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[14].uvs);
    startscr_buttonInfo[14].uvs[3][0] = -75.1f;
    startscr_buttonInfo[14].uvs[3][1] = 55.20f;
    startscr_buttonInfo[14].uvs[3][2] = 100.0f;
    ScaleUV(startscr_buttonInfo[14].uvs[0], startscr_buttonInfo[14].uvs[0], 0.5f);
    memcpy(startscr_buttonInfo[15].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[15].uvs);
    startscr_buttonInfo[15].uvs[3][0] = -87.1f;
    startscr_buttonInfo[15].uvs[3][1] = 55.20f;
    startscr_buttonInfo[15].uvs[3][2] = 100.0f;
    tex = MBOX_FindTexture_WORK("CURSOR", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[16].objIdx, tex, -1);
    startscr_buttonsInited = 1;
    startscr_buttonsJustInited = 1;
    ZeroCameras();
}

void startscr_uninitButtons(void) /* uninitButtons (non-static, named after its retail stub) */
{
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < 18; buttonIdx++)
        if (startscr_buttonInfo[buttonIdx].objIdx != -1) {
            ZOID_RemoveObject(startscr_buttonInfo[buttonIdx].objIdx);
            startscr_buttonInfo[buttonIdx].objIdx = -1;
        }
    startscr_buttonsInited = 0;
}

void startscr_updateButtons(void)
{
    f32 unused0, unused1, unused2;
    s32 buttonIdx;
    f32 prevPos[3];
    void *tex;
    u16 texIdx;
    f32 buttonY, selectedY;
    if (!startscr_buttonsInited)
        return;
    buttonY = startscr_maxY;
    selectedY = startscr_maxY;
    for (buttonIdx = 0; buttonIdx < 10; buttonIdx++) {
        if (!startscr_optionValid(buttonIdx)) {
            MBOX_HideObject(startscr_buttonInfo[buttonIdx].objIdx, 0, 15);
            continue;
        }
        MBOX_ShowObject(startscr_buttonInfo[buttonIdx].objIdx, 0, 15);
        if (buttonIdx == gStartMenuSelect) {
            if (startscr_buttonInfo[buttonIdx].angle < 3.1415927f)
                startscr_buttonInfo[buttonIdx].angle += 12.566371f * gFrameSeconds;
            if (startscr_buttonInfo[buttonIdx].angle > 3.1415927f || startscr_buttonsJustInited == 1)
                startscr_buttonInfo[buttonIdx].angle = 3.1415927f;
            selectedY = buttonY;
        } else {
            if (startscr_buttonInfo[buttonIdx].angle > 0.0f)
                startscr_buttonInfo[buttonIdx].angle -= 12.566371f * gFrameSeconds;
            if (startscr_buttonInfo[buttonIdx].angle < 0.0f || startscr_buttonsJustInited == 1)
                startscr_buttonInfo[buttonIdx].angle = 0.0f;
        }
        startscr_buttonInfo[buttonIdx].uvs[3][0] = startscr_buttonX;
        startscr_buttonInfo[buttonIdx].uvs[3][1] = buttonY;
        startscr_buttonInfo[buttonIdx].uvs[3][2] = startscr_buttonZ;
        ScaleUV(unitmatrix[0], startscr_buttonInfo[buttonIdx].uvs[0], startscr_gButtonScale);
        WPitchUV(startscr_buttonInfo[buttonIdx].angle - 1.5707964f, startscr_buttonInfo[buttonIdx].uvs[0]);
        if (startscr_buttonInfo[buttonIdx].angle > 1.5707964f)
            tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
        else
            tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
        ZOID_SetTexture(startscr_buttonInfo[buttonIdx].objIdx, tex, -1);
        if (buttonIdx == 0) {
            startscr_buttonInfo[10].angle = startscr_buttonInfo[buttonIdx].angle;
            startscr_buttonInfo[10].uvs[3][0] = startscr_buttonInfo[0].uvs[3][0] + 160.0f * startscr_gButtonScale;
            startscr_buttonInfo[10].uvs[3][1] = startscr_buttonInfo[0].uvs[3][1];
            startscr_buttonInfo[10].uvs[3][2] = startscr_buttonInfo[0].uvs[3][2];
            startscr_buttonInfo[10].uvs[3][0] = 1.02f * startscr_buttonInfo[10].uvs[3][0];
            startscr_buttonInfo[10].uvs[3][1] = 1.02f * startscr_buttonInfo[10].uvs[3][1];
            startscr_buttonInfo[10].uvs[3][2] = 1.02f * startscr_buttonInfo[10].uvs[3][2];
            ScaleUV(unitmatrix[0], startscr_buttonInfo[10].uvs[0], startscr_gButtonScale);
            WPitchUV(startscr_buttonInfo[10].angle - 1.5707964f, startscr_buttonInfo[10].uvs[0]);
            if (startscr_buttonInfo[10].angle > 1.5707964f)
                tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
            else
                tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(startscr_buttonInfo[10].objIdx, tex, -1);
        }
        buttonY -= startscr_gButtonSpace * startscr_gButtonScale;
    }
    memcpy(prevPos, startscr_buttonInfo[16].uvs[3], 12);
    startscr_buttonInfo[16].angle += 6.2831855f * gFrameSeconds;
    if (startscr_buttonInfo[16].angle > 6.2831855f)
        startscr_buttonInfo[16].angle -= 6.2831855f;
    startscr_buttonInfo[16].uvs[3][0] = startscr_buttonX * startscr_cursorZ / startscr_buttonZ + (-60.0f) * startscr_gButtonScale;
    startscr_buttonInfo[16].uvs[3][1] = selectedY * startscr_cursorZ / startscr_buttonZ;
    startscr_buttonInfo[16].uvs[3][2] = startscr_cursorZ;
    if (!startscr_buttonsJustInited) {
        if (60.0f * fabsf(startscr_buttonInfo[16].uvs[3][1] - prevPos[1]) / 10.0f > startscr_cursorSpeed) {
            startscr_cursorSpeed = 60.0f * fabsf(startscr_buttonInfo[16].uvs[3][1] - prevPos[1]) / 10.0f;
            startscr_cursorSpeed = startscr_cursorSpeed < 540.0f * startscr_gButtonScale ? startscr_cursorSpeed : 540.0f * startscr_gButtonScale;
            startscr_cursorSpeed = startscr_cursorSpeed > 180.0f * startscr_gButtonScale ? startscr_cursorSpeed : 180.0f * startscr_gButtonScale;
        }
        if (startscr_buttonInfo[16].uvs[3][1] > prevPos[1] + startscr_cursorSpeed * gFrameSeconds)
            startscr_buttonInfo[16].uvs[3][1] = prevPos[1] + startscr_cursorSpeed * gFrameSeconds;
        else if (startscr_buttonInfo[16].uvs[3][1] < prevPos[1] - startscr_cursorSpeed * gFrameSeconds)
            startscr_buttonInfo[16].uvs[3][1] = prevPos[1] - startscr_cursorSpeed * gFrameSeconds;
        else
            startscr_cursorSpeed = 180.0f * startscr_gButtonScale;
    }
    ScaleUV(unitmatrix[0], startscr_buttonInfo[16].uvs[0], 0.4f * startscr_gButtonScale);
    WRollUV(startscr_buttonInfo[16].angle, startscr_buttonInfo[16].uvs[0]);
    WPitchUV(1.5707964f, startscr_buttonInfo[16].uvs[0]);
    memcpy(prevPos, startscr_buttonInfo[17].uvs[3], 12);
    startscr_buttonInfo[17].uvs[3][0] = startscr_buttonX * startscr_cursorZ / startscr_buttonZ + (72.0f + gNumPlayers * 22.5f) * startscr_gButtonScale;
    startscr_buttonInfo[17].uvs[3][1] = startscr_maxY * startscr_cursorZ / startscr_buttonZ;
    startscr_buttonInfo[17].uvs[3][2] = startscr_cursorZ;
    if (!startscr_buttonsJustInited) {
        if (60.0f * fabsf(startscr_buttonInfo[17].uvs[3][0] - prevPos[0]) / 10.0f > playerCursorSpeed) {
            playerCursorSpeed = 60.0f * fabsf(startscr_buttonInfo[17].uvs[3][0] - prevPos[0]) / 10.0f;
            playerCursorSpeed = playerCursorSpeed < 540.0f * startscr_gButtonScale ? playerCursorSpeed : 540.0f * startscr_gButtonScale;
            playerCursorSpeed = playerCursorSpeed > 180.0f * startscr_gButtonScale ? playerCursorSpeed : 180.0f * startscr_gButtonScale;
        }
        if (startscr_buttonInfo[17].uvs[3][0] > prevPos[0] + playerCursorSpeed * gFrameSeconds)
            startscr_buttonInfo[17].uvs[3][0] = prevPos[0] + playerCursorSpeed * gFrameSeconds;
        else if (startscr_buttonInfo[17].uvs[3][0] < prevPos[0] - playerCursorSpeed * gFrameSeconds)
            startscr_buttonInfo[17].uvs[3][0] = prevPos[0] - playerCursorSpeed * gFrameSeconds;
        else
            playerCursorSpeed = 180.0f * startscr_gButtonScale;
    }
    if (startscr_buttonInfo[0].angle > 1.5707964f)
        tex = MBOX_FindTexture_WORK("CURSOR", &texIdx, 0, gNumModels - 1, 1);
    else
        tex = MBOX_FindTexture_WORK("CURSOR_B", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[17].objIdx, tex, -1);
    ScaleUVXYZ(unitmatrix[0], startscr_buttonInfo[17].uvs[0], 0.6f, 0.4f, 0.4f);
    ScaleUV(startscr_buttonInfo[17].uvs[0], startscr_buttonInfo[17].uvs[0], startscr_gButtonScale);
    WPitchUV(1.5707964f + startscr_buttonInfo[0].angle, startscr_buttonInfo[17].uvs[0]);
    startscr_buttonsJustInited = 0;
}
