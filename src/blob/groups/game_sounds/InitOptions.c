/* InitOptions: the options and race state at start-up (MAT3 options.c), and the players' input records. */
#include "game/game.h"

/* A player's input record (MAT3 tPadState) as this file defines it. */
typedef struct Player {
    u8 node;
    s8 unk1;
    u8 pad2[0x42 - 2];
    s8 unk42;
    u8 pad43;
    s32 unk44;
    void *ref;              /* 0x48: a pointer into gJustPlayers (Player76.ref), so 8 bytes and 80-byte records here */
} Player;

Player gPlayers[4];
VIEW(s8[], gOptions);
#define gOptions gOptions_view
VIEW(s8[4], gDriveHulk);
#define gDriveHulk gDriveHulk_view
VIEW(Rec164A4, gCheatState);
#define gCheatState gCheatState_view

/* One player in one window. */
static void reset_player_counts(void) {
    gNumWindows = 1;
    gNumPlayers = 1;
}

/* Gives every option its default (GetDefaultOption) and sets the game from them: the volumes and mono, the HUD's
 * parts, the race settings, the stunt and battle settings; clears every cheat; one player in one window. The input
 * records' defaults too. */
void InitOptions(void) {
    s32 i;

    {
        int j;
        for (j = 0; j < 4; j++) {
            gPlayers[j].unk1 = 5;
            gPlayers[j].unk42 = 0;
            gPlayers[j].unk44 = -1;
            gPlayers[j].node = j;
        }
    }
    SetControls(1);
    for (i = 0; i < kNumOptions; i++) {
        gOptions[i] = GetDefaultOption(i);
    }
    AudSetMasterVol(gOptions[kOptEffectsVolume] / 10.0f, 0.05f, 0, 1);
    AudSetMasterVol(gOptions[kOptMusicVolume] / 10.0f, 0.05f, 1, 0);
    AudMono(gOptions[kOptMono]);
    gNumLaps = gOptions[kOptLaps];
    gNumDrones = gOptions[kOptDrones];
    gShowTrackMap = gOptions[kOptTrackMap];
    gGravityScale = gOptions[kOptGravity];
    gShowRadar = gOptions[kOptRadar];
    gShowElapsed = gOptions[kOptElapsed];
    gShowRemaining = gOptions[kOptRemaining];
    gShowTach = gOptions[kOptTach];
    gShowSpeed = gOptions[kOptSpeed];
    gShowOdometer = gOptions[kOptOdometer];
    gShowPlace = gOptions[kOptPlace];
    gShowShifter = gOptions[kOptShifter];
    gDifficulty = gOptions[kOptDifficulty];
    gGhostSelect = gOptions[kOptGhostSelect];
    gCatchup = gOptions[kOptCatchup];
    gBackwards = gOptions[kOptBackwards];
    gMirrorWaves = gOptions[kOptMirror] > 0;
    gMirrorControls = gOptions[kOptMirror] >= 2;
    gFogIntensity = gOptions[kOptFog];
    gWind = gOptions[kOptWind];
    gShowMetric = gOptions[kOptMetric];
    gDeathMode = gOptions[kOptDeathMode];
    gShowPlayerArrows = gOptions[kOptPlayerArrows];
    gShowCoins = gOptions[kOptCoins];
    gLang = gOptions[kOptLanguage];
    gShowWrongWay = gOptions[kOptWrongWay];
    gStuntMode = gOptions[kOptStuntMode];
    gStuntFlow = gOptions[kOptStuntFlow];
    gStuntModeMins = gOptions[kOptStuntMinutes];
    gStuntModePoints = gOptions[kOptStuntPoints];
    gStuntModeDeaths = gOptions[kOptStuntDeaths];
    gBattleMode = gOptions[kOptBattleMode];
    gBattleModeMins = gOptions[kOptBattleMinutes];
    gBattleModePoints = gOptions[kOptBattlePoints];
    gNoCarCollisions = 0;
    gFogColor = 0;
    gMineCones = 0;
    gCarMines = 0;
    gCheatState.unk06 = 0;
    gUpsideDownTrack = 0;
    gNoAutoAbort = 0;
    gInvincible = 0;
    gInvisibleCar = 0;
    gInvisibleTrack = 0;
    gNoBrakes = 0;
    gSpecialTires = 0;
    gMassiveMass = 0;
    gSuicideMode = 0;
    gCheatState.unk1A = 0;
    gCheatState.unk1C = 0;
    gCheatState.unk1E = 0;
    gCheatState.unk20 = 0;
    gSuperSpeed = 0;
    gResurrectInPlace = 0;
    gFrameScale = 0;
    gEnableTireScaling = 0;
    for (i = 0; i < 4; i++) {
        gDriveHulk[i] = 0;
    }
    reset_player_counts();
}
