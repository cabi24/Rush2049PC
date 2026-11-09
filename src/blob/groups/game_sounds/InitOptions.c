#include "game/game.h"

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

static void reset_player_counts(void) {
    gNumWindows = 1;
    gNumPlayers = 1;
}

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
    for (i = 0; i < 41; i++) {
        gOptions[i] = GetDefaultOption(i);
    }
    AudSetMasterVol(gOptions[13] / 10.0f, 0.05f, 0, 1);
    AudSetMasterVol(gOptions[12] / 10.0f, 0.05f, 1, 0);
    AudMono(gOptions[15]);
    gNumLaps = gOptions[21];
    gNumDrones = gOptions[22];
    gShowTrackMap = gOptions[0];
    gGravityScale = gOptions[23];
    gShowRadar = gOptions[1];
    gShowElapsed = gOptions[2];
    gShowRemaining = gOptions[3];
    gShowTach = gOptions[4];
    gShowSpeed = gOptions[5];
    gShowOdometer = gOptions[6];
    gShowPlace = gOptions[7];
    gShowShifter = gOptions[8];
    gDifficulty = gOptions[24];
    gGhostSelect = gOptions[25];
    gCatchup = gOptions[26];
    gBackwards = gOptions[27];
    gMirrorWaves = gOptions[28] > 0;
    gMirrorControls = gOptions[28] >= 2;
    gFogIntensity = gOptions[29];
    gWind = gOptions[30];
    gShowMetric = gOptions[9];
    gDeathMode = gOptions[31];
    gShowPlayerArrows = gOptions[10];
    gShowCoins = gOptions[11];
    gLang = gOptions[16];
    gShowWrongWay = gOptions[18];
    gStuntMode = gOptions[32];
    gStuntFlow = gOptions[33];
    gStuntModeMins = gOptions[35];
    gStuntModePoints = gOptions[34];
    gStuntModeDeaths = gOptions[36];
    gBattleMode = gOptions[37];
    gBattleModeMins = gOptions[39];
    gBattleModePoints = gOptions[38];
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
