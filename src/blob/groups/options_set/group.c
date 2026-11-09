/* SetOptions: the game takes the options screen's choices (MAT3 options.c). */
#include "game/game.h"

typedef struct Stats { u8 pad[1785]; u8 option_a; u8 pad1786; u8 option_b; } Stats;
typedef struct Profile { Stats *stats; } Profile;
typedef struct PlayerState { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; Profile *profile; } PlayerState;
typedef struct Player { PlayerState *state; } Player;
typedef struct InputRecord { u8 pad[72]; Player *player; } InputRecord;
extern InputRecord gPlayers[];

VIEW(s8[], gOptions);
#define gOptions gOptions_view
VIEW(u8[][5], gCircuitTracks);
#define gCircuitTracks gCircuitTracks_view

/* Saves the game options to every player's profile (not in attract mode) and sets the game from them: the HUD's
 * parts, and the race settings of the race type (gRace) from its options, or fixed ones in attract mode (3 laps, 6
 * drones, no mirror, fog or wind, normal gravity, difficulty and catch-up 2). */
void SetOptions(void)
{
    s32 i, setting;
    s8 configuration_mode;
    u8 *options;
    Player *player;
    if (!(gGameState & GS_DEMO)) {
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
    if (gGameState & GS_DEMO) {
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
    if (gGameState & GS_FRONTEND_MASK) {
        gMirrorWaves = 0;
        gMirrorControls = 0;
    }
}
