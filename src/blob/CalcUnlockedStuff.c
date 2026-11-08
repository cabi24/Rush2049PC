/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * CalcUnlockedStuff @ 0x800F2A28, 2,728 bytes (from -> best.c, 9/682).
 * Names are historical labels.
 *
 * Unlock-table refresh, no networking: for each of gNumPlayers players it fills five s8 tables from the save
 * record reached through gPlayers[i].ref->car->data->base (medal bit counts per track/cup, summed track
 * values / 10 against thresholds, summed cup values and stunt counts against thresholds, three mode records),
 * with three cheat words (cheat_all_cars/C2/C4 == 1) that set everything. CountBits is a bit count.
 * No arcade ancestor found.
 *
 * Quirks this match depends on (each measured; see and the. notes):
 *  - section 3: `total = 0` before the 19-wide zero loop (row pointer in a1);
 *  - section 4 reads expansion_pak_present directly in the `if`s (no `flag` local);
 *  - section 4's loop is written `i = 0; if (i < gNumPlayers) { do { ... } while (++i < gNumPlayers);
 *    if (data4) {} }`: an empty (compiled-out) check of data4 after the loop. It extends data4's live range
 *    past the loop, which lowers its colouring priority below the gForceUnlockedCircuits address (retail a3 vs a2).
 *    Inside the guard data4 is defined on every path to the check, so uopt adds no home store; the same check
 * after a plain `for` loop costs a store and a stack slot. Probably a debug print in the original.
 *  - the cheat branch clears gUnlockedCircuits[i][2] with i == player count (one row past the last player), as retail.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct {
    /* 0x00 */ u8 pad0[0x58];
    /* 0x58 */ s32 unk58;
    /* 0x5C */ u16 unk5C;
    /* 0x5E */ u16 unk5E;
} TrackRec; /* 0x60 */

typedef struct {
    /* 0x00 */ u8 pad0[0xC];
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ u8 pad10[0x2C];
    /* 0x3C */ u16 unk3C;
    /* 0x3E */ u16 unk3E;
} CupRec; /* 0x40 */

typedef struct {
    /* 0x00 */ u8 pad0[8];
    /* 0x08 */ u16 unk8;
    /* 0x0A */ u16 unkA;
} StuntRec; /* 0x0C */

typedef struct {
    /* 0x00 */ u16 unk0;
    /* 0x02 */ u16 unk2;
    /* 0x04 */ u16 unk4;
    /* 0x06 */ u16 unk6;
    /* 0x08 */ u8 pad8[0x14];
} ModeRec; /* 0x1C */

typedef struct {
    /* 0x000 */ u8 pad0[0x8C];
    /* 0x08C */ TrackRec tracks[12];
    /* 0x50C */ CupRec cups[4];
    /* 0x60C */ StuntRec stunts[8];
    /* 0x66C */ ModeRec modes[4];
} SaveBase;

typedef struct {
    /* 0x00 */ SaveBase *base;
} Data;

typedef struct {
    /* 0x00 */ void *next; void *prev; void *fileh; u8 pad0_C[0x2C - 0xC];
    /* 0x2C */ Data *data;
} Car;

typedef struct {
    /* 0x00 */ Car *car;
} Ref;

typedef struct {
    /* 0x00 */ u8 pad0[0x48];
    /* 0x48 */ Ref *ref;
} Player76; /* 0x4C */

extern s16 cheat_all_cars;
extern s16 cheat_all_tracks;
extern s16 cheat_all_parts;
extern s16 gNumPlayers;
extern Player76 gPlayers[];
extern s8 gUnlockedTracks[][19];
extern s8 gUnlockedCars[][13];
extern s8 gUnlockedCircuits[][4];
extern s8 gUnlockedTrans[][8];
extern s8 gUnlockedEngines[][9];
extern s8 gUnlockedTires[][5];
extern s8 gUnlockedFrames[][6];
extern s8 gForceUnlockedCircuits[];
extern s8 expansion_pak_present;

extern s32 CountBits(u32 x, u8 n);

void CalcUnlockedStuff(void) {
    s16 i;
    s16 j;
    s32 k;
    s32 n0;
    s32 n1;
    s32 n2;
    s32 n3;
    Player76 *p;
    TrackRec *track;
    CupRec *cup;
    s32 sum;
    Data *data;
    s32 total;
    s32 total2;
    CupRec *cup2;
    StuntRec *stunt;
    s32 q;
    Data *data4;
    SaveBase *base;
    s8 flag;

    if (cheat_all_cars == 1) {
        for (i = 0; i < gNumPlayers; i++) {
            for (j = 0; j < 13; j++) {
                gUnlockedCars[i][j] = 1;
            }
        }
    } else {
        for (i = 0; i < gNumPlayers; i++) {
            p = &gPlayers[i];
            n0 = 0;
            n1 = 0;
            n2 = 0;
            n3 = 0;
            if (p->ref->car->data == 0) {
                continue;
            }
            for (j = 0; j < 13; j++) {
                if (j < 6) {
                    gUnlockedCars[i][j] = 1;
                } else {
                    gUnlockedCars[i][j] = 0;
                }
            }
            for (k = 0; k != 6; k++) {
                track = &p->ref->car->data->base->tracks[k];
                n0 += CountBits(track->unk5C & 0xFF, 16);
                n1 += CountBits(track->unk5C & 0xFF00, 16);
            }
            for (k = 0; k != 4; k++) {
                cup = &p->ref->car->data->base->cups[k];
                n2 += CountBits(cup->unk3C & 0xFF, 16);
                n3 += CountBits(cup->unk3C & 0xFF00, 16);
            }
            if (n0 >= 48) {
                gUnlockedCars[i][6] = 1;
            }
            if (n1 >= 24) {
                gUnlockedCars[i][7] = 1;
            }
            if (n1 >= 36) {
                gUnlockedCars[i][8] = 1;
            }
            if (n2 >= 32) {
                gUnlockedCars[i][9] = 1;
            }
            if (n3 >= 16) {
                gUnlockedCars[i][10] = 1;
            }
            if (n3 >= 24) {
                gUnlockedCars[i][11] = 1;
            }
            if (n0 >= 48 && n1 >= 48 && n2 >= 32 && n3 >= 32) {
                gUnlockedCars[i][12] = 1;
            }
        }
    }

    if (cheat_all_parts == 1) {
        for (i = 0; i < gNumPlayers; i++) {
            if (gPlayers[i].ref->car->data == 0) {
                continue;
            }
            for (j = 0; j < 8; j++) {
                if (j == 6 || j == 7) {
                    gUnlockedTrans[i][j] = 0;
                } else {
                    gUnlockedTrans[i][j] = 1;
                }
            }
            for (j = 0; j < 9; j++) {
                gUnlockedEngines[i][j] = 1;
            }
            for (j = 0; j < 5; j++) {
                gUnlockedTires[i][j] = 1;
            }
            for (j = 0; j < 6; j++) {
                gUnlockedFrames[i][j] = 1;
            }
        }
    } else {
        for (i = 0; i < gNumPlayers; i++) {
            sum = 0;
            data = gPlayers[i].ref->car->data;
            if (data == 0) {
                continue;
            }
            for (k = 0; k < 12; k++) {
                sum += data->base->tracks[k].unk58;
            }
            sum /= 10;
            gUnlockedTrans[i][0] = 1;
            gUnlockedTrans[i][1] = 1;
            gUnlockedTrans[i][2] = sum >= 200;
            gUnlockedTrans[i][3] = sum >= 200;
            gUnlockedTrans[i][4] = sum >= 500;
            gUnlockedTrans[i][5] = sum >= 500;
            gUnlockedTrans[i][6] = 0;
            gUnlockedTrans[i][7] = 0;
            gUnlockedEngines[i][0] = 1;
            gUnlockedEngines[i][1] = 1;
            gUnlockedEngines[i][2] = 1;
            gUnlockedEngines[i][3] = sum >= 250;
            gUnlockedEngines[i][4] = sum >= 500;
            gUnlockedEngines[i][5] = sum >= 800;
            gUnlockedEngines[i][6] = sum >= 1200;
            gUnlockedEngines[i][7] = sum >= 1600;
            gUnlockedEngines[i][8] = sum >= 2000;
            gUnlockedTires[i][0] = 1;
            gUnlockedTires[i][1] = sum >= 300;
            gUnlockedTires[i][2] = sum >= 1200;
            gUnlockedTires[i][3] = sum >= 100;
            gUnlockedTires[i][4] = sum >= 600;
            gUnlockedFrames[i][0] = 1;
            gUnlockedFrames[i][1] = 1;
            gUnlockedFrames[i][2] = 1;
            gUnlockedFrames[i][3] = sum >= 150;
            gUnlockedFrames[i][4] = sum >= 400;
            gUnlockedFrames[i][5] = sum >= 700;
        }
    }

    if (cheat_all_tracks == 1) {
        for (i = 0; i < gNumPlayers; i++) {
            for (j = 0; j < 19; j++) {
                gUnlockedTracks[i][j] = 1;
            }
        }
    } else {
        for (i = 0; i < gNumPlayers; i++) {
            total2 = 0;
            total = 0;
            for (j = 0; j < 19; j++) {
                gUnlockedTracks[i][j] = 0;
            }
            gUnlockedTracks[i][0] = 1;
            gUnlockedTracks[i][1] = 1;
            gUnlockedTracks[i][2] = 1;
            gUnlockedTracks[i][3] = 1;
            gUnlockedTracks[i][14] = 1;
            gUnlockedTracks[i][6] = 1;
            gUnlockedTracks[i][7] = 1;
            gUnlockedTracks[i][8] = 1;
            gUnlockedTracks[i][9] = 1;
            data = gPlayers[i].ref->car->data;
            if (data == 0) {
                continue;
            }
            cup2 = data->base->cups;
            stunt = data->base->stunts;
            for (k = 0; k < 4; k++) {
                total += cup2[k].unk0C;
            }
            for (q = 0; q < 8; q++) {
                total2 += stunt[q].unk8;
            }
            gUnlockedTracks[i][15] = total >= 100000;
            gUnlockedTracks[i][16] = total >= 250000;
            gUnlockedTracks[i][17] = total >= 500000;
            gUnlockedTracks[i][18] = total >= 1000000;
            gUnlockedTracks[i][10] = total2 >= 100;
            gUnlockedTracks[i][11] = total2 >= 250;
            gUnlockedTracks[i][12] = total2 >= 500;
            gUnlockedTracks[i][13] = total2 >= 1000;
        }
    }

    if (cheat_all_tracks == 1) {
        for (i = 0; i < gNumPlayers; i++) {
            for (j = 0; j < 4; j++) {
                gUnlockedCircuits[i][j] = 1;
            }
        }
        if (expansion_pak_present == 0) {
            gUnlockedCircuits[i][2] = 0;
        }
    } else {
        i = 0;
        if (i < gNumPlayers) {
            do {
                data4 = gPlayers[i].ref->car->data;
                if (data4 == 0) {
                    continue;
                }
                base = data4->base;
                for (j = 0; j < 4; j++) {
                    gUnlockedCircuits[i][j] = 0;
                }
                gUnlockedCircuits[i][0] = 1;
                if (base->modes[1].unk2 != 0 || base->modes[1].unk4 != 0 || base->modes[1].unk6 != 0 || gForceUnlockedCircuits[1] != 0) {
                    gUnlockedCircuits[i][1] = 1;
                    gUnlockedTracks[i][4] = 1;
                }
                if (expansion_pak_present == 0) {
                    if (base->modes[2].unk2 != 0 || base->modes[2].unk4 != 0 || base->modes[2].unk6 != 0 || gForceUnlockedCircuits[3] != 0) {
                        gUnlockedCircuits[i][3] = 1;
                    }
                } else {
                    if (base->modes[2].unk2 != 0 || base->modes[2].unk4 != 0 || base->modes[2].unk6 != 0 || gForceUnlockedCircuits[2] != 0) {
                        gUnlockedCircuits[i][2] = 1;
                        gUnlockedTracks[i][5] = 1;
                    }
                    if (base->modes[3].unk2 != 0 || base->modes[3].unk4 != 0 || base->modes[3].unk6 != 0 || gForceUnlockedCircuits[3] != 0) {
                        gUnlockedCircuits[i][3] = 1;
                    }
                }
            } while (++i < gNumPlayers);
            if (data4) {}
        }
    }
}
