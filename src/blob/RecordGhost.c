/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * RecordGhost (0x800F4FEC, 1236 B): historical label. This is the N64 end-of-race records update
 * (reconstruction). For each player (count gNumPlayers, Player records gPlayers stride 0x4C):
 * resolve the cached profile pointer (else &gJustPlayers[slot]); stop if the profile has no save block;
 * then for the profile's own record (*(*prof)->save)->rec[track] and the global gTotalRaceStats[track]:
 * insert the car's finish time into the ascending 5-entry times[] (for the global set also shift the
 * owner bytes gNewTotalScore[n+9 -> n+10] and owner pointers D_80151690[track].who[]), bump races/u54,
 * add laps, the lap-time sum gLapTimes[i][0..laps) and car distance / 528.0f. Finally
 * SavePlayerStats(prof, (u8)track2). With the mirror flag gBackwards, track += 6 and track2 += 19.
 *
 * Source-form notes (no shaping devices, no unused locals):
 * - track/track2 are each assigned from gTrackNum (retail keeps the loaded byte as its own value in v0,
 *   copies it to track and track2, and forms +6 from it); track2 += 19 reads the variable (ugen temp ring).
 * - car is assigned the record address, the finished test indexes gGameCar[] directly and the time read
 *   goes through car: retail computes the address in v0 and copies it to car (move s5,v0).
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct Record {        /* 0x60 per track */
    u8 pad00[0x2C];
    f32 times[5];              /* 0x2C best times, ascending */
    f32 total;                 /* 0x40 */
    u16 laps;                  /* 0x44 */
    u16 races;                 /* 0x46 */
    u8 pad48[0x4E - 0x48];
    u16 u4E;                   /* 0x4E */
    u8 pad50[0x54 - 0x50];
    u16 u54;                   /* 0x54 */
    u8 pad56[0x58 - 0x56];
    u32 dist;                  /* 0x58 */
    u8 pad5C[0x60 - 0x5C];
} Record;

typedef struct SaveBlock {
    u8 pad00[0x8C];
    Record rec[1];             /* 0x8C */
} SaveBlock;

typedef struct Profile {
    void *next, *prev;
    void *valid;               /* the pak file handle (pc_prelude.h) */
    u8 pad0C[0x2C - 0x0C];
    SaveBlock **save;          /* 0x2C */
} Profile;

/* gTotalNames on the PC: every slot is pointer-sized, a name index or a profile handle (docs/port/CASTS.md). */
typedef struct Owner {         /* 0x3C per track (0x78 on the PC) */
    intptr_t names[2][5];
    Profile **who[5];          /* 0x28 */
} Owner;

typedef struct Player {        /* 0x4C */
    u8 car;                    /* 0x00 */
    u8 slot;                   /* 0x01 */
    u8 pad02[0x40 - 0x02];
    u16 laps;                  /* 0x40 */
    u8 pad42[0x48 - 0x42];
    Profile **prof;            /* 0x48 */
} Player;

typedef struct CarData {       /* 0x3B8 */
    u8 pad00[0xEF];
    s8 finished;               /* 0xEF */
    f32 time;                  /* 0xF0 */
    u8 padF4[0x108 - 0xF4];
    f32 dist;                  /* 0x108 */
    u8 pad10C[0x110 - 0x10C]; struct pc_visual visuals[21]; u8 pad10C_308[0x380 - 0x308]; void *input; u8 pad10C_384[0x3B8 - 0x384];
} CarData;

extern s8 gTrackNum;          /* track */
extern s8 gBackwards;          /* mirror / reverse flag */
extern s16 gNumPlayers;         /* player count */
extern Player gPlayers[];
extern Profile *gJustPlayers[];
extern u8 gLap[];        /* laps driven per player */
extern f32 gLapTimes[][8];    /* lap times per player */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Record gTotalRaceStats[];
extern Owner gTotalNames[];
extern s8 gNewTotalScore[];
void SavePlayerStats(Profile **prof, u8 track);

void RecordGhost(void)
{
    s32 track, track2;
    s32 i, j, k, n;
    Record *r;
    CarData *car;
    f32 time;

    track = gTrackNum;
    track2 = gTrackNum;
    if (gBackwards) {
        track = gTrackNum + 6;
        track2 += 19;
    }
    for (i = 0; i < gNumPlayers; i++) {
        if (gPlayers[i].prof == 0)
            gPlayers[i].prof = &gJustPlayers[gPlayers[i].slot];
        if ((*gPlayers[i].prof)->save == 0)
            return;
        for (j = 0; j < 2; j++) {
            if (j == 0)
                r = &(*(*gPlayers[i].prof)->save)->rec[track];
            else
                r = &gTotalRaceStats[track];
            car = PC_CAR(gPlayers[i].car);
            if (PC_CAR_FINISHED(gPlayers[i].car)) {
                time = car->time;
                for (k = 0; k < 5; k++) {
                    if (r->times[k] == 0.0f || time < r->times[k]) {
                        for (n = 4; n > k; n--) {
                            r->times[n] = r->times[n - 1];
                            if (j == 1) {
                                gNewTotalScore[n + 10] = gNewTotalScore[n + 9];
                                gTotalNames[track].who[n] = gTotalNames[track].who[n - 1];
                            }
                        }
                        r->times[k] = time;
                        if (j == 1) {
                            gNewTotalScore[n + 10] = i;
                            if ((*gPlayers[i].prof)->valid)
                                gTotalNames[track].who[k] = gPlayers[i].prof;
                        }
                        break;
                    }
                }
            }
            r->races++;
            r->u54++;
            r->u4E += gPlayers[i].laps;
            r->laps += gLap[i];
            for (n = 0; n < gLap[i]; n++)
                r->total += gLapTimes[i][n];
            r->dist += car->dist / 528.0f;
        }
        SavePlayerStats(gPlayers[i].prof, track2);
    }
}
