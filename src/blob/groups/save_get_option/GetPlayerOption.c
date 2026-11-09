/* GetPlayerOption and the race settings it reads: a profile's options (MAT3 save.c). */
#include "game/game.h"

/* A battle setting of the profile (option 23, 28..30, 37..40: SavePlayerOption's numbering), 0 for another option. */
s8 GetBattleSetting(tProfile **profile, u8 selector) {
    tSaveData *fields = *(*profile)->datah;

    switch (selector) {
    case kOptGravity: return fields->battleSettings.gravity;
    case kOptMirror: return fields->battleSettings.mirror;
    case kOptFog: return fields->battleSettings.fog;
    case kOptWind: return fields->battleSettings.wind;
    case kOptBattleMode: return fields->battleSettings.mode;
    case kOptBattlePoints: return fields->battleSettings.points;
    case kOptBattleMinutes: return fields->battleSettings.minutes;
    case kOptBattleNeg: return fields->battleSettings.neg;
    default: return 0;
    }
}

/* A stunt setting of the profile (23, 28..30, 32..36), 0 for another option. */
s8 GetStuntSetting(tProfile **profile, u8 selector) {
    tSaveData *fields = *(*profile)->datah;

    switch (selector) {
    case kOptGravity: return fields->stuntSettings.gravity;
    case kOptMirror: return fields->stuntSettings.mirror;
    case kOptFog: return fields->stuntSettings.fog;
    case kOptWind: return fields->stuntSettings.wind;
    case kOptStuntMode: return fields->stuntSettings.mode;
    case kOptStuntFlow: return fields->stuntSettings.flow;
    case kOptStuntPoints: return fields->stuntSettings.points;
    case kOptStuntMinutes: return fields->stuntSettings.minutes;
    case kOptStuntDeaths: return fields->stuntSettings.deaths;
    default: return 0;
    }
}

/* An obstacle course setting of the profile (23, 28..30), 0 for another option. */
s8 GetObstacleSetting(tProfile **profile, u8 selector) {
    tSaveData *fields = *(*profile)->datah;

    switch (selector) {
    case kOptGravity: return fields->obstacleSettings.gravity;
    case kOptMirror: return fields->obstacleSettings.mirror;
    case kOptFog: return fields->obstacleSettings.fog;
    case kOptWind: return fields->obstacleSettings.wind;
    default: return 0;
    }
}

/* A race setting of the profile (21..24, 26..31), 0 for another option; mode is unused. */
s8 GetRaceSetting(tProfile **profile, u8 selector, s32 mode) {
    tSaveData *fields = *(*profile)->datah;

    switch (selector) {
    case kOptLaps: return fields->raceSettings.laps;
    case kOptDrones: return fields->raceSettings.drones;
    case kOptGravity: return fields->raceSettings.gravity;
    case kOptDifficulty: return fields->raceSettings.difficulty;
    case kOptCatchup: return fields->raceSettings.catchup;
    case kOptBackwards: return fields->raceSettings.backwards;
    case kOptMirror: return fields->raceSettings.mirror;
    case kOptFog: return fields->raceSettings.fog;
    case kOptWind: return fields->raceSettings.wind;
    case kOptDeathMode: return fields->raceSettings.deaths;
    default: return 0;
    }
}

/* Option selector of the profile's file, which must be read: a game option (0..20), or a setting of the race type of
 * the track gTrackNum (race tracks 0..5 and 19..24, battle arenas 6..13, stunt arenas 14..17, else the obstacle
 * course). */
s32 GetPlayerOption(tProfile **profile, u8 selector) {
    s32 mode;

    if (selector < kNumGameOptions) {
        return (*(*profile)->datah)->options[selector];
    }
    mode = gTrackNum;
    if (mode >= 0 && mode < 6) {
        return GetRaceSetting(profile, selector, mode);
    }
    if (mode >= 19 && mode < 25) {
        return GetRaceSetting(profile, selector, mode);
    }
    if (mode >= 6 && mode < 14) {
        return GetBattleSetting(profile, selector);
    }
    if (mode >= 14 && mode < 18) {
        return GetStuntSetting(profile, selector);
    }
    return GetObstacleSetting(profile, selector);
}
