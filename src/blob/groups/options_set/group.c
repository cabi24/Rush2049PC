/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/* Apply the saved configuration to active player profiles, then initialize
 * the runtime options for the selected gameplay mode. The mode-word bit 8
 * path selects fixed defaults without changing the player profiles.
 *
 * Native N64 evidence: the profile-setting helper consumes (Player *, u8,
 * s8); configuration is a signed-byte array, and the input records are
 * 76 bytes with their player pointer at offset 72. Mode 3 additionally reads
 * a five-byte option row and the selected player's two saved option bytes.
 *
 * Arcade search: rushtherock/game/game.c has an EEPROM-driven game-options
 * reader, but no direct equivalent of this N64 player-profile/mode switch
 * was identified. The seven switch destinations are independently checked
 * against the retail table at 0x80123F98 by the group relocation verifier.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct Stats { u8 pad[1785]; u8 option_a; u8 pad1786; u8 option_b; } Stats;
typedef struct Profile { Stats *stats; } Profile;
typedef struct PlayerState { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; Profile *profile; } PlayerState;
typedef struct Player { PlayerState *state; } Player;
typedef struct InputRecord { u8 pad[72]; Player *player; } InputRecord;
extern InputRecord gPlayers[];
extern s16 gNumPlayers;
extern u32 gGameState;
extern s32 gRace;
extern s8 gOptions[];
extern u8 gCircuitTracks[][5];
extern s8 gCircuitTrack;
extern u8 gCircuitPlayer;
void SavePlayerOption(Player *, u8, s8);
extern s8 gShowTrackMap;
extern s8 gShowRadar;
extern s8 gShowElapsed;
extern s8 gShowRemaining;
extern s8 gShowTach;
extern s8 gShowSpeed;
extern s8 gShowPlace;
extern s8 gShowShifter;
extern s8 gShowMetric;
extern s8 gShowPlayerArrows;
extern s8 gShowWrongWay;
extern s8 gBackwards;
extern s8 gMirrorWaves;
extern s8 gMirrorControls;
extern s8 gFogIntensity;
extern s8 gWind;
extern s8 gGravityScale;
extern s8 gDifficulty;
extern s8 gCatchup;
extern s8 gDeathMode;
extern s8 gShowOdometer;
extern s8 gShowCoins;
extern s8 gGhostSelect;
extern s16 gNumLaps;
extern s16 gNumDrones;
extern s32 gStuntMode;
extern s32 gStuntFlow;
extern s32 gStuntModePoints;
extern s32 gStuntModeMins;
extern s32 gStuntModeDeaths;
extern s32 gBattleMode;
extern s32 gBattleModePoints;
extern s32 gBattleModeMins;
void SetOptions(void)
{
    s32 i, setting;
    s8 configuration_mode;
    u8 *options;
    Player *player;
    if (!(gGameState & 8)) {
        for (i = 0; i < gNumPlayers; i++) {
            if (gPlayers[i].player != 0) {
                for (setting = 0; setting < 21; setting++) {
                    SavePlayerOption(gPlayers[i].player, (u8)setting, gOptions[setting]);
                }
            }
        }
    }
    gShowTrackMap = gOptions[0];
    gShowRadar = gOptions[1];
    gShowElapsed = gOptions[2];
    gShowRemaining = gOptions[3];
    gShowTach = gOptions[4];
    gShowSpeed = gOptions[5];
    gShowPlace = gOptions[7];
    gShowShifter = gOptions[8];
    gShowMetric = gOptions[9];
    gShowPlayerArrows = gOptions[10];
    gShowWrongWay = gOptions[18];
    if (gGameState & 8) {
        gNumLaps = 3;
        gNumDrones = 6;
        gBackwards = 0;
        gMirrorWaves = 0;
        gMirrorControls = 0;
        gFogIntensity = 0;
        gWind = 0;
        gGravityScale = 1;
        gDifficulty = 2;
        gCatchup = 2;
        gDeathMode = 0;
    } else {
        switch (gRace) {
        case 0:
            gNumLaps = gOptions[21];
            gNumDrones = gOptions[22];
            if (gNumPlayers >= 3) gNumDrones = 0;
            gBackwards = gOptions[27];
            configuration_mode = gOptions[28];
            gMirrorWaves = configuration_mode > 0;
            gMirrorControls = configuration_mode >= 2;
            gFogIntensity = gOptions[29];
            gWind = gOptions[30];
            gGravityScale = gOptions[23];
            gDifficulty = gOptions[24];
            gCatchup = gOptions[26];
            gDeathMode = gOptions[31];
            gShowOdometer = gOptions[6];
            gShowCoins = gOptions[11];
            break;
        case 1:
            gNumLaps = 1;
            gNumDrones = 0;
            gBackwards = gOptions[27];
            configuration_mode = gOptions[28];
            gMirrorWaves = configuration_mode > 0;
            gMirrorControls = configuration_mode >= 2;
            gFogIntensity = gOptions[29];
            gWind = gOptions[30];
            gGravityScale = gOptions[23];
            gDifficulty = gOptions[24];
            gCatchup = gOptions[26];
            gDeathMode = 0;
            gShowOdometer = gOptions[6];
            gShowCoins = gOptions[11];
            break;
        case 2:
            gNumLaps = 3;
            gNumDrones = 0;
            gBackwards = gOptions[27];
            gMirrorWaves = 0;
            gMirrorControls = 0;
            gFogIntensity = gOptions[29];
            gWind = 0;
            gGravityScale = 1;
            gDifficulty = 2;
            gGhostSelect = gOptions[25];
            gCatchup = 0;
            gDeathMode = 0;
            gShowOdometer = gOptions[6];
            gShowCoins = gOptions[11];
            break;
        case 3:
            gNumLaps = 3;
            gNumDrones = 5;
            options = gCircuitTracks[gCircuitTrack];
            gBackwards = options[1];
            gMirrorWaves = options[2];
            gMirrorControls = 0;
            gFogIntensity = options[3];
            gWind = options[4];
            gGravityScale = 1;
            player = gPlayers[gCircuitPlayer].player;
            gDifficulty = player->state->profile->stats->option_b;
            gCatchup = 2;
            gDeathMode = player->state->profile->stats->option_a;
            gShowOdometer = gOptions[6];
            gShowCoins = gOptions[11];
            break;
        case 4:
            gNumLaps = 1;
            gNumDrones = 0;
            gBackwards = 0;
            configuration_mode = gOptions[28];
            gMirrorWaves = configuration_mode > 0;
            gMirrorControls = configuration_mode >= 2;
            gFogIntensity = 0;
            gWind = gOptions[30];
            gGravityScale = gOptions[23];
            gDifficulty = 0;
            gCatchup = 0;
            gDeathMode = 0;
            gStuntMode = gOptions[32];
            gStuntFlow = gOptions[33];
            gStuntModePoints = gOptions[34];
            gStuntModeMins = gOptions[35];
            gStuntModeDeaths = gOptions[36];
            gShowTrackMap = 0;
            gShowOdometer = 0;
            gShowCoins = gOptions[11];
            break;
        case 5:
            gNumLaps = 1;
            gNumDrones = 0;
            gBackwards = 0;
            configuration_mode = gOptions[28];
            gMirrorWaves = configuration_mode > 0;
            gMirrorControls = configuration_mode >= 2;
            gFogIntensity = 0;
            gWind = 0;
            gGravityScale = gOptions[23];
            gDifficulty = 0;
            gCatchup = 0;
            gDeathMode = 0;
            gShowTrackMap = 0;
            gShowOdometer = 0;
            gShowCoins = 0;
            break;
        case 6:
            gNumLaps = 1;
            gNumDrones = 0;
            gBackwards = 0;
            configuration_mode = gOptions[28];
            gMirrorWaves = configuration_mode > 0;
            gMirrorControls = configuration_mode >= 2;
            gFogIntensity = 0;
            gWind = gOptions[30];
            gGravityScale = gOptions[23];
            gDifficulty = 0;
            gCatchup = 0;
            gDeathMode = 0;
            /* Preserve the saved-mode reset before reading its word option. */
            gShowSpeed = 0;
            gBattleMode = gOptions[37];
            gBattleModePoints = gOptions[38];
            gBattleModeMins = gOptions[39];
            gShowTrackMap = 0;
            gShowOdometer = 0;
            gShowCoins = 0;
            break;
        }
    }
    if (gGameState & 0x7C03FFFE) {
        gMirrorWaves = 0;
        gMirrorControls = 0;
    }
}
