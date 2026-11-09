/* SavePlayerOption and the race settings it saves: a profile's options and per-race-type settings (MAT3 save.c). */
#include "game/game.h"

void PS_WriteFile(tPakHandle *fileh, void *data, int size);

/* Saves race setting option (21..31) of the profile: value into its race settings, and when it changed, their
 * checksum again and the settings marked to write. Other options are not race settings and are ignored. track is
 * unused. */
void SaveRaceSetting(tProfile **player, unsigned char option, signed char value, unsigned char track)
{
    tSaveData *data = *(*player)->datah;
    signed char *setting;
    switch (option) {
    case kOptLaps: setting = &data->raceSettings.laps; break;
    case kOptDrones: setting = &data->raceSettings.drones; break;
    case kOptGravity: setting = &data->raceSettings.gravity; break;
    case kOptDifficulty: setting = &data->raceSettings.difficulty; break;
    case kOptCatchup: setting = &data->raceSettings.catchup; break;
    case kOptBackwards: setting = &data->raceSettings.backwards; break;
    case kOptMirror: setting = &data->raceSettings.mirror; break;
    case kOptFog: setting = &data->raceSettings.fog; break;
    case kOptWind: setting = &data->raceSettings.wind; break;
    case kOptDeathMode: setting = &data->raceSettings.deaths; break;
    default: return;
    }
    if (*setting != value) {
        *setting = value;
        data->raceSettings.checksum = CalcChecksumInternal(&data->raceSettings.track, sizeof(data->raceSettings) - 4);
        PS_WriteFile((*player)->fileh, &data->raceSettings, sizeof(data->raceSettings));
    }
}

/* The same for a battle setting (23, 28..30, 37..40). */
static void _SaveBattleSetting(tProfile **player, unsigned char option, signed char value, int track)
{
    tSaveData *data = *(*player)->datah;
    signed char *setting;
    switch (option) {
    case kOptGravity: setting = &data->battleSettings.gravity; break;
    case kOptMirror: setting = &data->battleSettings.mirror; break;
    case kOptFog: setting = &data->battleSettings.fog; break;
    case kOptWind: setting = &data->battleSettings.wind; break;
    case kOptBattleMode: setting = &data->battleSettings.mode; break;
    case kOptBattlePoints: setting = &data->battleSettings.points; break;
    case kOptBattleMinutes: setting = &data->battleSettings.minutes; break;
    case kOptBattleNeg: setting = &data->battleSettings.neg; break;
    default: return;
    }
    if (*setting != value) {
        *setting = value;
        data->battleSettings.checksum = CalcChecksumInternal(&data->battleSettings.track, sizeof(data->battleSettings) - 4);
        PS_WriteFile((*player)->fileh, &data->battleSettings, sizeof(data->battleSettings));
    }
}

/* The same for a stunt setting (23, 28..30, 32..36). */
static void _SaveStuntSetting(tProfile **player, unsigned char option, signed char value, int track)
{
    tSaveData *data = *(*player)->datah;
    signed char *setting;
    switch (option) {
    case kOptGravity: setting = &data->stuntSettings.gravity; break;
    case kOptMirror: setting = &data->stuntSettings.mirror; break;
    case kOptFog: setting = &data->stuntSettings.fog; break;
    case kOptWind: setting = &data->stuntSettings.wind; break;
    case kOptStuntMode: setting = &data->stuntSettings.mode; break;
    case kOptStuntFlow: setting = &data->stuntSettings.flow; break;
    case kOptStuntPoints: setting = &data->stuntSettings.points; break;
    case kOptStuntMinutes: setting = &data->stuntSettings.minutes; break;
    case kOptStuntDeaths: setting = &data->stuntSettings.deaths; break;
    default: return;
    }
    if (*setting != value) {
        *setting = value;
        data->stuntSettings.checksum = CalcChecksumInternal(&data->stuntSettings.track, sizeof(data->stuntSettings) - 4);
        PS_WriteFile((*player)->fileh, &data->stuntSettings, sizeof(data->stuntSettings));
    }
}

/* The same for an obstacle course setting (23, 28..30). */
static void _SaveObstacleSetting(tProfile **player, unsigned char option, signed char value, int track)
{
    tSaveData *data = *(*player)->datah;
    signed char *setting;
    switch (option) {
    case kOptGravity: setting = &data->obstacleSettings.gravity; break;
    case kOptMirror: setting = &data->obstacleSettings.mirror; break;
    case kOptFog: setting = &data->obstacleSettings.fog; break;
    case kOptWind: setting = &data->obstacleSettings.wind; break;
    default: return;
    }
    if (*setting != value) {
        *setting = value;
        data->obstacleSettings.checksum = CalcChecksumInternal(&data->obstacleSettings.track, sizeof(data->obstacleSettings) - 4);
        PS_WriteFile((*player)->fileh, &data->obstacleSettings, sizeof(data->obstacleSettings));
    }
}

/* Saves option (kOpt*) of the profile's file as value, writing it when it changed: a game option, or
 * a setting of gTrackNum's race type (tracks 0..5 and 19..24 race, 6..13 battle, 14..17 stunt, the rest the obstacle
 * course). Nothing for a profile whose file is not read. */
void SavePlayerOption(tProfile **player, unsigned char option, signed char value)
{
    tSaveData *data;
    if ((*player)->datah == 0) return;
    data = *(*player)->datah;
    if (option < kNumGameOptions) {
        if (data->options[option] != value) {
            data->options[option] = value;
            data->optChecksum = CalcChecksumInternal(data->options, kNumGameOptions);
            PS_WriteFile((*player)->fileh, &data->optChecksum, 25);   /* the checksum and the options */
        }
    } else if (gTrackNum >= 0 && gTrackNum < 6) SaveRaceSetting(player, option, value, gTrackNum);
    else if (gTrackNum >= 19 && gTrackNum < 25) SaveRaceSetting(player, option, value, gTrackNum);
    else if (gTrackNum >= 6 && gTrackNum < 14) _SaveBattleSetting(player, option, value, (unsigned char)(gTrackNum - 6));
    else if (gTrackNum >= 14 && gTrackNum < 18) _SaveStuntSetting(player, option, value, (unsigned char)(gTrackNum - 14));
    else _SaveObstacleSetting(player, option, value, (unsigned char)(gTrackNum - 18));
}
