/* UpdateArrowVis: which battle players can see each other (MAT3 visuals.c). Each
 * frame of a battle with the player arrows shown, a car's arrow over another player's car is faded where a wall
 * stands between them (gFadePlayerArrow, read by the arrow's animator). */
#include "game/game.h"

/* How far above the cars' positions the line of sight runs, in world units. */
#define SIGHT_HEIGHT 5.0f

/* Clears gFadePlayerArrow, then in a battle with the arrows shown marks both ways each pair of players whose cars
 * have a wall between them (a ray from one, SIGHT_HEIGHT up, to the other hits the world).
 * Original game: the inner loop starts at 1, not i + 1, so each pair is tested twice and each car against itself
 * once (as in MAT3). */
void UpdateArrowVis(void) {
    s32 i, j, k;
    f32 from[3], to[3];
    f32 hit_matrix[9];

    for (i = 0; i < 4; i++)
        for (k = 0; k < 4; k++)
            gFadePlayerArrow[i][k] = 0;
    if (gShowPlayerArrows && gRace == kBattle) {
        for (i = 0; i < gNumPlayers - 1; i++) {
            from[0] = pc_cars[i].dr_pos[0];
            from[1] = pc_cars[i].dr_pos[1];
            from[2] = pc_cars[i].dr_pos[2];
            from[1] += SIGHT_HEIGHT;
            for (j = 1; j < gNumPlayers; j++) {
                to[0] = pc_cars[j].dr_pos[0];
                to[1] = pc_cars[j].dr_pos[1];
                to[2] = pc_cars[j].dr_pos[2];
                to[1] += SIGHT_HEIGHT;
                if (ShotToWorldCollision(from, to, hit_matrix, 0.5f, 0, 0xFFFF)) {
                    gFadePlayerArrow[i][j] = 1;
                    gFadePlayerArrow[j][i] = 1;
                }
            }
        }
    }
}
