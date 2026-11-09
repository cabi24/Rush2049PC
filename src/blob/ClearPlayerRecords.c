/* ClearPlayerRecords: erase a profile's records (MAT3 save.c). */
#include "game/game.h"

extern void *memset(void *s, s32 c, u32 n);

void PS_WriteFile(tPakHandle *fileh, void *data, s32 size);

/* Erases the profile's records and totals (the race, stunt, battle, obstacle and circuit stats and the circuit in
 * progress) in its file and writes the file. Returns 1 when done; 0 when the file can't be read, or can't be written
 * (its data is then dropped from memory). The profile's times leave the high score tables. */
s32 ClearPlayerRecords(tProfile **profile) {
    tSaveData *data;

    if (ReadPlayerData(profile) == 0) {
        return 0;
    }
    data = *(*profile)->datah;
    memset(data->raceStats, 0, sizeof(data->raceStats));
    PS_WriteFile((*profile)->fileh, data->raceStats, sizeof(data->raceStats));
    memset(data->stuntStats, 0, sizeof(data->stuntStats));
    PS_WriteFile((*profile)->fileh, data->stuntStats, sizeof(data->stuntStats));
    memset(data->battleStats, 0, sizeof(data->battleStats));
    PS_WriteFile((*profile)->fileh, data->battleStats, sizeof(data->battleStats));
    memset(data->obstacleStats, 0, sizeof(data->obstacleStats));
    PS_WriteFile((*profile)->fileh, data->obstacleStats, sizeof(data->obstacleStats));
    memset(data->circuitStats, 0, sizeof(data->circuitStats));
    PS_WriteFile((*profile)->fileh, data->circuitStats, sizeof(data->circuitStats));
    memset(&data->circuit, 0, sizeof(data->circuit));
    PS_WriteFile((*profile)->fileh, &data->circuit, sizeof(data->circuit));
    if (PS_FlushFile((*profile)->fileh, 0) == 0) {
        PurgePlayerData(profile);
        return 0;
    }
    RemovePlayerScores((intptr_t) profile);
    PS_FlushPak((*profile)->fileh->file->pak, 1);
    return 1;
}
