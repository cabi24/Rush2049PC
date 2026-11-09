#include "game/game.h"

typedef struct TrackTimes {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ f32 times[3][5];
    /* 0x40 */ u8 pad40[0x20];
} TrackTimes; /* 0x60 */

typedef struct SaveData {
    /* 0x00 */ u8 pad0[0x8C];
    /* 0x8C */ TrackTimes tracks[12];
} SaveData;

typedef struct SaveSlot {
    /* 0x00 */ SaveData *data;
} SaveSlot;

typedef struct Owner {
    /* 0x00 */ void *next; void *prev; void *fileh; u8 pad0_C[0x2C - 0xC];
    /* 0x2C */ SaveSlot *slot;
} Owner;

typedef struct Entry {
    /* 0x00 */ Owner *owner;
} Entry;

VIEW(TrackTimes[12], gTotalRaceStats);
#define gTotalRaceStats gTotalRaceStats_view
VIEW(Entry *[12][3][5], gTotalNames);
#define gTotalNames gTotalNames_view

void AddPlayerScores(Entry *entry)
{
    SaveData *data;
    s32 i;
    s32 j;
    s32 k;
    s32 m;
    s32 l;
    f32 *src;
    f32 *times;
    Entry **owners;

    if (entry->owner->slot == 0) {
        return;
    }
    data = entry->owner->slot->data;
    for (i = 0; i < 12; i++) {
        for (j = 0; j < 3; j++) {
            src = data->tracks[i].times[j];
            l = 0;
            times = gTotalRaceStats[i].times[j];
            owners = gTotalNames[i][j];
            for (k = 0; k < 5; k++) {
                if (0.0f < src[l]) {
                    if (0.0f == times[k] || src[l] < times[k]) {
                        for (m = 4; m > k; m--) {
                            times[m] = times[m - 1];
                            owners[m] = owners[m - 1];
                        }
                        times[k] = src[l];
                        owners[m] = entry;
                        l++;
                    }
                }
            }
        }
    }
}
