/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (-O3 required: at -O2 the slot-pointer web takes s0 and the frame grows, 136/144) */
/*
 * End-of-race statistics update for the current player (gCircuitPlayer), N64-only.
 * Two 28-byte records are updated with the same code: first the one inside
 * the player's save data (base + 0x684 + sel * 0x1C, sel = s8 at base+0x6FC),
 * then the session copy gTotalCircuitStats[sel].  Per record: total++, one of three
 * counters by gCircuitCars.unk1 (0/1/2), two more counters when gDeathMode is
 * set (the second only for unk1 < 3), sum += gCircuitCars.unk2, best =
 * max(best, unk2), and bestTime = base->unk704 when bestTime is 0 or greater
 * than it.  Finally SavePlayerCircuitStats(ref, sel) re-checksums the save record
 * (it stores CalcChecksumInternal(rec+4, 24) at rec+0 and marks the slot).
 *
 * `ref` (Player76 +0x48) points into the pointer table gJustPlayers and is
 * filled in lazily from the byte at +1, as in IsCoinFound.
 *
 * What mattered (no quirks): every access is spelled
 * gPlayers[gCircuitPlayer].ref, never through a local `Player76 *p`.  The
 * value of that expression is then one CSE web for the whole function (t1 at
 * the top and again in front of the final call), which pushes the constants
 * to t1/t2 as in retail.  With a pointer local the top and the end are two
 * webs (v1 and a0) and 20 words differ.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;

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

typedef struct {
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

extern u8 gCircuitPlayer;
extern Player76 gPlayers[];
extern Car *gJustPlayers[];
extern Rec gTotalCircuitStats[];
extern s8 gDeathMode;
extern Result gCircuitCars;

void SavePlayerCircuitStats(Car **ref, u8 idx);

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
