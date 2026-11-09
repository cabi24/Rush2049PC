#include "game/game.h"

typedef struct {
    /* 0x00 */ s32 checksum;
    /* 0x04 */ u16 total;
    /* 0x06 */ u16 count[3];
    /* 0x0C */ u16 unkC;
    /* 0x0E */ u16 unkE;
    /* 0x10 */ u16 best;
    /* 0x14 */ f32 bestTime;
    /* 0x18 */ s32 sum;
} Rec; /* 0x1C */

typedef struct {
    /* 0x000 */ u8 pad0[0x684];
    /* 0x684 */ Rec recs[4];
    /* 0x6F4 */ u8 pad6F4[8];
    /* 0x6FC */ s8 unk6FC;
    /* 0x700 */ f32 unk700;
    /* 0x704 */ f32 unk704;
} Base;

typedef struct {
    Base *base;
} Data;

typedef struct Car {
    void *next; void *prev; void *fileh; u8 pad0_C[0x2C - 0xC];
    Data *data;
} Car;

typedef struct {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 index;
    /* 0x02 */ u8 pad2[0x46];
    /* 0x48 */ Car **ref;
} Player76; /* 0x4C */

typedef struct {
    u8 unk0;
    u8 unk1;
    u16 unk2;
} Result;

extern Player76 gPlayers[];
VIEW(Car *[], gJustPlayers);
#define gJustPlayers gJustPlayers_view
VIEW(Rec[], gTotalCircuitStats);
#define gTotalCircuitStats gTotalCircuitStats_view

VIEW(Result, gCircuitCars);
#define gCircuitCars gCircuitCars_view

void RecordCircuit(void) {
    s32 i;
    s32 idx;
    Rec *rec;

    if (gPlayers[gCircuitPlayer].ref == 0) {
        gPlayers[gCircuitPlayer].ref = &gJustPlayers[gPlayers[gCircuitPlayer].index];
    }
    idx = (*gPlayers[gCircuitPlayer].ref)->data->base->unk6FC;
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            rec = &(*gPlayers[gCircuitPlayer].ref)->data->base->recs[idx];
        } else {
            rec = &gTotalCircuitStats[idx];
        }
        rec->total++;
        if (gCircuitCars.unk1 == 0) {
            rec->count[0]++;
        } else if (gCircuitCars.unk1 == 1) {
            rec->count[1]++;
        } else if (gCircuitCars.unk1 == 2) {
            rec->count[2]++;
        }
        if (gDeathMode) {
            rec->unkC++;
            if (gCircuitCars.unk1 < 3) {
                rec->unkE++;
            }
        }
        rec->sum += gCircuitCars.unk2;
        if (rec->best < gCircuitCars.unk2) {
            rec->best = gCircuitCars.unk2;
        }
        if (rec->bestTime == 0.0f || (*gPlayers[gCircuitPlayer].ref)->data->base->unk704 < rec->bestTime) {
            rec->bestTime = (*gPlayers[gCircuitPlayer].ref)->data->base->unk704;
        }
    }
    SavePlayerCircuitStats(gPlayers[gCircuitPlayer].ref, (*gPlayers[gCircuitPlayer].ref)->data->base->unk6FC);
}
