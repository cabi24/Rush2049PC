/* CalcUnlockedStuff: what each player has unlocked (MAT3 save.c). */
#include "game/game.h"

/* A player's input record as this reads it: the profile handle in use. */
typedef struct {
    /* 0x00 */ u8 pad0[0x48];
    /* 0x48 */ tProfile **profile;
} PlayerPad; /* 0x4C */

extern PlayerPad gPlayers[];
VIEW(s8[][19], gUnlockedTracks);
#define gUnlockedTracks gUnlockedTracks_view
VIEW(s8[][13], gUnlockedCars);
#define gUnlockedCars gUnlockedCars_view

VIEW(s8[][6], gUnlockedFrames);
#define gUnlockedFrames gUnlockedFrames_view
VIEW(s8[], gForceUnlockedCircuits);
#define gForceUnlockedCircuits gForceUnlockedCircuits_view

/* Fills the unlock tables (gUnlockedCars, gUnlockedTrans, gUnlockedEngines, gUnlockedTires, gUnlockedFrames,
 * gUnlockedTracks, gUnlockedCircuits) for each player from its profile, or all of them with the cheats:
 *   cars 6..12     by the coins found: on the race tracks (the low byte's coins, raceCoins; the high byte's,
 *                  raceCoins2) and on the stunt arenas (stuntCoins, stuntCoins2)
 *   the parts      by the miles driven over all tracks (totalMiles is in tenths)
 *   tracks         race tracks 0..3, battle arenas 6..9 and stunt arena 14 always; battle arenas 10..13 by the kills,
 *                  stunt arenas 15..17 and the obstacle course (18) by the stunt points; race tracks 4 and 5 with
 *                  the circuits that unlock them
 *   circuits       the first always; each next one by a top-three finish of the one before (or gForceUnlockedCircuits);
 *                  without the Expansion Pak the third circuit is skipped
 * A player whose profile's file isn't read keeps what it had. */
void CalcUnlockedStuff(void) {
    s16 i;
    s16 j;
    s32 k;
    s32 raceCoins;
    s32 raceCoins2;
    s32 stuntCoins;
    s32 stuntCoins2;
    PlayerPad *p;
    tRaceStats *track;
    tStuntStats *arena;
    s32 miles;
    tSaveData **datah;
    s32 stuntPoints;
    s32 kills;
    tStuntStats *arenas;
    tBattleStats *battles;
    s32 q;
    tSaveData **datah4;
    tSaveData *base;
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
            raceCoins = 0;
            raceCoins2 = 0;
            stuntCoins = 0;
            stuntCoins2 = 0;
            if ((*p->profile)->datah == 0) {
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
                track = &(*(*p->profile)->datah)->raceStats[k];
                raceCoins += CountBits(track->coinsFound & 0xFF, 16);
                raceCoins2 += CountBits(track->coinsFound & 0xFF00, 16);
            }
            for (k = 0; k != 4; k++) {
                arena = &(*(*p->profile)->datah)->stuntStats[k];
                stuntCoins += CountBits(arena->coinsFound & 0xFF, 16);
                stuntCoins2 += CountBits(arena->coinsFound & 0xFF00, 16);
            }
            if (raceCoins >= 48) {
                gUnlockedCars[i][6] = 1;
            }
            if (raceCoins2 >= 24) {
                gUnlockedCars[i][7] = 1;
            }
            if (raceCoins2 >= 36) {
                gUnlockedCars[i][8] = 1;
            }
            if (stuntCoins >= 32) {
                gUnlockedCars[i][9] = 1;
            }
            if (stuntCoins2 >= 16) {
                gUnlockedCars[i][10] = 1;
            }
            if (stuntCoins2 >= 24) {
                gUnlockedCars[i][11] = 1;
            }
            if (raceCoins >= 48 && raceCoins2 >= 48 && stuntCoins >= 32 && stuntCoins2 >= 32) {
                gUnlockedCars[i][12] = 1;
            }
        }
    }

    if (cheat_all_parts == 1) {
        for (i = 0; i < gNumPlayers; i++) {
            if ((*gPlayers[i].profile)->datah == 0) {
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
            miles = 0;
            datah = (*gPlayers[i].profile)->datah;
            if (datah == 0) {
                continue;
            }
            for (k = 0; k < 12; k++) {
                miles += (*datah)->raceStats[k].totalMiles;
            }
            miles /= 10;
            gUnlockedTrans[i][0] = 1;
            gUnlockedTrans[i][1] = 1;
            gUnlockedTrans[i][2] = miles >= 200;
            gUnlockedTrans[i][3] = miles >= 200;
            gUnlockedTrans[i][4] = miles >= 500;
            gUnlockedTrans[i][5] = miles >= 500;
            gUnlockedTrans[i][6] = 0;
            gUnlockedTrans[i][7] = 0;
            gUnlockedEngines[i][0] = 1;
            gUnlockedEngines[i][1] = 1;
            gUnlockedEngines[i][2] = 1;
            gUnlockedEngines[i][3] = miles >= 250;
            gUnlockedEngines[i][4] = miles >= 500;
            gUnlockedEngines[i][5] = miles >= 800;
            gUnlockedEngines[i][6] = miles >= 1200;
            gUnlockedEngines[i][7] = miles >= 1600;
            gUnlockedEngines[i][8] = miles >= 2000;
            gUnlockedTires[i][0] = 1;
            gUnlockedTires[i][1] = miles >= 300;
            gUnlockedTires[i][2] = miles >= 1200;
            gUnlockedTires[i][3] = miles >= 100;
            gUnlockedTires[i][4] = miles >= 600;
            gUnlockedFrames[i][0] = 1;
            gUnlockedFrames[i][1] = 1;
            gUnlockedFrames[i][2] = 1;
            gUnlockedFrames[i][3] = miles >= 150;
            gUnlockedFrames[i][4] = miles >= 400;
            gUnlockedFrames[i][5] = miles >= 700;
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
            kills = 0;
            stuntPoints = 0;
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
            datah = (*gPlayers[i].profile)->datah;
            if (datah == 0) {
                continue;
            }
            arenas = (*datah)->stuntStats;
            battles = (*datah)->battleStats;
            for (k = 0; k < 4; k++) {
                stuntPoints += arenas[k].totalPoints;
            }
            for (q = 0; q < 8; q++) {
                kills += battles[q].totalKills;
            }
            gUnlockedTracks[i][15] = stuntPoints >= 100000;
            gUnlockedTracks[i][16] = stuntPoints >= 250000;
            gUnlockedTracks[i][17] = stuntPoints >= 500000;
            gUnlockedTracks[i][18] = stuntPoints >= 1000000;
            gUnlockedTracks[i][10] = kills >= 100;
            gUnlockedTracks[i][11] = kills >= 250;
            gUnlockedTracks[i][12] = kills >= 500;
            gUnlockedTracks[i][13] = kills >= 1000;
        }
    }

    if (cheat_all_tracks == 1) {
        for (i = 0; i < gNumPlayers; i++) {
            for (j = 0; j < 4; j++) {
                gUnlockedCircuits[i][j] = 1;
            }
        }
        /* Original game: i is gNumPlayers here, after the loop, so this clears circuit 2 one row past the last
         * player (with four players, past the end of gUnlockedCircuits[4][4]) and leaves the players' rows unlocked
         * without the Expansion Pak. The N64 code does so (MAT3 has no such line). Kept as retail. */
        if (expansion_pak_present == 0) {
            gUnlockedCircuits[i][2] = 0;
        }
    } else {
        i = 0;
        if (i < gNumPlayers) {
            do {
                datah4 = (*gPlayers[i].profile)->datah;
                if (datah4 == 0) {
                    continue;
                }
                base = *datah4;
                for (j = 0; j < 4; j++) {
                    gUnlockedCircuits[i][j] = 0;
                }
                gUnlockedCircuits[i][0] = 1;
                if (base->circuitStats[0].num1st != 0 || base->circuitStats[0].num2nd != 0 || base->circuitStats[0].num3rd != 0 || gForceUnlockedCircuits[1] != 0) {
                    gUnlockedCircuits[i][1] = 1;
                    gUnlockedTracks[i][4] = 1;
                }
                if (expansion_pak_present == 0) {
                    if (base->circuitStats[1].num1st != 0 || base->circuitStats[1].num2nd != 0 || base->circuitStats[1].num3rd != 0 || gForceUnlockedCircuits[3] != 0) {
                        gUnlockedCircuits[i][3] = 1;
                    }
                } else {
                    if (base->circuitStats[1].num1st != 0 || base->circuitStats[1].num2nd != 0 || base->circuitStats[1].num3rd != 0 || gForceUnlockedCircuits[2] != 0) {
                        gUnlockedCircuits[i][2] = 1;
                        gUnlockedTracks[i][5] = 1;
                    }
                    if (base->circuitStats[2].num1st != 0 || base->circuitStats[2].num2nd != 0 || base->circuitStats[2].num3rd != 0 || gForceUnlockedCircuits[3] != 0) {
                        gUnlockedCircuits[i][3] = 1;
                    }
                }
            } while (++i < gNumPlayers);
            if (datah4) {}
        }
    }
}
