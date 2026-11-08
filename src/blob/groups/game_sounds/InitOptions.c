/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * InitOptions, settings/race reset, 0x800DE45C, 1020 bytes.
 * Initializes four 76-byte player records, reads 41 option bytes, applies
 * two AudSetMasterVol calls, copies options, and clears per-race state.
 *
 * Native declaration ownership and reset_player_counts helper boundary are
 * inferred N64 source hypotheses, not authenticated MAT3 donor identities.
 * The two global definitions describe existing image BSS objects; the group
 * relocator resolves them to gPlayers and gDriveHulk and emits code only.
 * Their object-file adjacency is NOT the native image layout.
 * The helper contains the real final player/window count stores and fully
 * inlines. There are no fake calls, unused locals, volatile additions or
 * synthetic storage. A scoped first loop counter enables native unrolling;
 * reuse of i in the final loop and its statement-line separation affect
 * uopt/as1 scheduling. See and the bounded negative controls.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;

typedef struct {
    u8 node;
    s8 unk1;
    u8 pad2[0x42 - 2];
    s8 unk42;
    u8 pad43;
    s32 unk44;
    void *ref;              /* 0x48: a pointer into gJustPlayers (Player76.ref), so 8 bytes and 80-byte records here */
} Player;

/* PC: the cheat flags gCheatState are s16[21] (cheatscr.c); the PC loader fills initialized data by its declarations,
 * and bytes here, or past the end, would stay big-endian (docs/port/ENDIAN.md). */
typedef struct {
    s16 unk00[3];
    s16 unk06;
    s16 unk08[9];
    s16 unk1A;
    s16 unk1C;
    s16 unk1E;
    s16 unk20;
    s16 unk22[4];
} Rec164A4;

Player gPlayers[4];
extern s8 gOptions[];
extern s8 gDriveHulk[4];
extern Rec164A4 gCheatState;
extern s16 gNumWindows;
extern s16 gNumPlayers;
extern s8 gFogIntensity;
extern s8 gMirrorControls;
extern s8 gNoBrakes;
extern s16 gLang;
extern s8 gMineCones;
extern s8 gCarMines;
extern s8 gNoAutoAbort;
extern s8 gInvisibleCar;
extern s8 gInvisibleTrack;
extern s8 gResurrectInPlace;
extern s32 gStuntMode;
extern s8 gEnableTireScaling;
extern s32 gStuntFlow;
extern s32 gStuntModeMins;
extern s32 gStuntModePoints;
extern s8 gMirrorWaves;
extern s32 gStuntModeDeaths;
extern s32 gBattleMode;
extern s32 gBattleModeMins;
extern s32 gBattleModePoints;
extern s8 gGhostSelect;
extern s16 gNumDrones;
extern s8 gWind;
extern s8 gDeathMode;
extern s8 gFogColor;
extern s8 gSuperSpeed;
extern s8 gInvincible;
extern s8 gSpecialTires;
extern s8 gMassiveMass;
extern s8 gCatchup;
extern s8 gUpsideDownTrack;
extern s8 gDifficulty;
extern s8 gBackwards;
extern s16 gNumLaps;
extern s8 gGravityScale;
extern s8 gShowTrackMap;
extern s8 gShowRadar;
extern s8 gShowElapsed;
extern s8 gShowRemaining;
extern s8 gShowTach;
extern s8 gShowSpeed;
extern s8 gShowOdometer;
extern s8 gShowPlace;
extern s8 gShowShifter;
extern s8 gShowWrongWay;
extern s8 gShowPlayerArrows;
extern s8 gShowCoins;
extern s8 gShowMetric;
extern s8 gFrameScale;
extern s8 gNoCarCollisions;
extern s8 gSuicideMode;

void SetControls(s32 arg0);
s8 GetDefaultOption(s32 i);
void AudSetMasterVol(f32 blend, f32 amount, s32 first, s32 second);
void AudMono(s8 arg0);

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
