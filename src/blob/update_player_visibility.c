#include "game/game.h"

VIEW(s8[4][4], gFadePlayerArrow);
#define gFadePlayerArrow gFadePlayerArrow_view

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

u16 *ShotToWorldCollision(void *arg0, void *arg1, f32 *arg2, f32 arg3, s32 arg4, s32 arg5);

void update_player_visibility(void) {
    s32 i, j, k;
    f32 first[3], second[3];
    f32 result[9];

    for (i = 0; i < 4; i++)
        for (k = 0; k < 4; k++)
            gFadePlayerArrow[i][k] = 0;
    if (gShowPlayerArrows && gRace == 6) {
        for (i = 0; i < gNumPlayers - 1; i++) {
            first[0] = PC_CAR_POS(i)[0];
            first[1] = PC_CAR_POS(i)[1];
            first[2] = PC_CAR_POS(i)[2];
            first[1] += 5.0f;
            for (j = 1; j < gNumPlayers; j++) {
                second[0] = PC_CAR_POS(j)[0];
                second[1] = PC_CAR_POS(j)[1];
                second[2] = PC_CAR_POS(j)[2];
                second[1] += 5.0f;
                if (ShotToWorldCollision(first, second, result, 0.5f, 0, 0xFFFF)) {
                    gFadePlayerArrow[i][j] = 1;
                    gFadePlayerArrow[j][i] = 1;
                }
            }
        }
    }
}
