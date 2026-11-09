#include "game/game.h"

typedef struct RaceStats {
    u32 unknown00;
    float samples[5];
    float means[5];
    u8 unknown44[20];
    float sample_sum;
    u16 sample_count;
    u16 finished_count;
    u16 first_count;
    u16 second_count;
    u16 third_count;
    u16 event_count;
    u16 flag_count;
    u16 flag_all_samples_count;
    u32 unknown84;
    u32 distance;
    u16 bits;
    u8 unknown94[2];
} RaceStats;

typedef struct StatsPayload {
    u8 *data;
} StatsPayload;
typedef struct ResourceRoot {
    void *next, *prev;
    void *enabled;      /* the profile's pak file handle (pc_prelude.h) */
    u8 unknown12[32];
    StatsPayload *payload;
} ResourceRoot;
typedef ResourceRoot **ResourceHandle;
typedef struct PlayerRecord {
    u8 car_index;
    u8 resource_index;
    u8 unknown02[62];
    u16 event_count;
    u8 unknown66[6];
    ResourceHandle resource;
} PlayerRecord;
typedef struct CarRecord {
    u8 unknown00[238];
    s8 place;
    s8 finished;
    u8 unknown240[24];
    float distance;
    u8 unknown268[0x110 - 0x10C]; struct pc_visual visuals[21]; u8 unknown268_308[0x380 - 0x308]; void *input; u8 unknown268_384[0x3B8 - 0x384];
} CarRecord;

extern PlayerRecord gPlayers[];
VIEW(ResourceRoot *[], gJustPlayers);
#define gJustPlayers gJustPlayers_view

VIEW(RaceStats[], gTotalRaceStats);
#define gTotalRaceStats gTotalRaceStats_view
VIEW(ResourceHandle[][15], gTotalNames);
#define gTotalNames gTotalNames_view

void RecordRace(void)
{
    s32 course;
    s32 resource_course;
    s32 player;
    s32 phase;
    CarRecord *car;
    s32 lap;
    s32 rank;
    s32 shift;
    float mean;
    RaceStats *stats;

    course = gTrackNum;
    resource_course = gTrackNum;
    if (gBackwards) {
        course += 6;
        resource_course += 19;
    }
    for (player = 0; player < gNumPlayers; player++) {
        if (gPlayers[player].resource == 0) {
            gPlayers[player].resource = &gJustPlayers[gPlayers[player].resource_index];
        }
        if ((*gPlayers[player].resource)->payload == 0) {
            return;
        }
        for (phase = 0; phase < 2; phase++) {
            if (phase == 0) {
                stats = (RaceStats *)((*gPlayers[player].resource)->payload->data + 140) + course;
            } else {
                stats = &gTotalRaceStats[course];
            }
            car = PC_CAR(gPlayers[player].car_index);
            for (lap = 0; lap < gLap[player]; lap++) {
                for (rank = 0; rank < 5; rank++) {
                    if (stats->samples[rank] == 0.0f ||
                        gLapTimes[player][lap] < stats->samples[rank]) {
                        for (shift = 4; shift > rank; shift--) {
                            stats->samples[shift] = stats->samples[shift - 1];
                            if (phase == 1) {
                                gNewTotalScore[shift] = gNewTotalScore[shift - 1];
                                gTotalNames[course][shift] = gTotalNames[course][shift - 1];
                            }
                        }
                        stats->samples[rank] = gLapTimes[player][lap];
                        if (phase == 1) {
                            gNewTotalScore[rank] = player;
                            if ((*gPlayers[player].resource)->enabled) {
                                gTotalNames[course][rank] = gPlayers[player].resource;
                            }
                        }
                        break;
                    }
                }
            }
            if (gLap[player] > 0) {
                mean = 0;
                for (shift = 0; shift < gLap[player]; shift++) {
                    mean += gLapTimes[player][shift];
                }
                mean /= (u32)gLap[player];
                for (rank = 0; rank < 5; rank++) {
                    if (stats->means[rank] == 0.0f || mean < stats->means[rank]) {
                        for (shift = 4; shift > rank; shift--) {
                            stats->means[shift] = stats->means[shift - 1];
                            if (phase == 1) {
                                gNewTotalScore[shift + 5] = gNewTotalScore[shift + 4];
                                gTotalNames[course][shift + 5] = gTotalNames[course][shift + 4];
                            }
                        }
                        stats->means[rank] = mean;
                        if (phase == 1) {
                            gNewTotalScore[shift + 5] = player;
                            if ((*gPlayers[player].resource)->enabled) {
                                gTotalNames[course][rank + 5] = gPlayers[player].resource;
                            }
                        }
                        break;
                    }
                }
            }
            if (car->finished) {
                stats->finished_count++;
                if (car->place == 0 && gNumActiveCars >= 2) {
                    stats->first_count++;
                } else if (car->place == 1 && gNumActiveCars >= 3) {
                    stats->second_count++;
                } else if (car->place == 2 && gNumActiveCars >= 4) {
                    stats->third_count++;
                }
                if (gDeathMode) {
                    stats->flag_count++;
                    if (gNumLaps == gLap[player]) {
                        stats->flag_all_samples_count++;
                    }
                }
            }
            stats->event_count += gPlayers[player].event_count;
            stats->sample_count += gLap[player];
            for (shift = 0; shift < gLap[player]; shift++) {
                stats->sample_sum += gLapTimes[player][shift];
            }
            stats->distance += car->distance / 528.0f;
        }
        SavePlayerStats(gPlayers[player].resource, resource_course);
    }
}
