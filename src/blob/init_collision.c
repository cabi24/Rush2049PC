/* init_collision.c: clear a car's collision state (MAT3 collision.c, init_collision()).
 *
 * Part of a car's set-up (syminit), at the race start and after each respawn. */
#include "game/game.h"

/* Clear model `m`'s centre force, its peak body and centre forces and each corner's body force, and start each
 * tire's road search from the root of the road tree (stree_nodes, MAT3 stree). */
void init_collision(tModelData *m) {
    int i, j;
    for (i = 0; i < 3; i++) {
        m->centerForce[i] = 0.0f;
        m->peak_center_force[0][i] = 0.0f;
        m->peak_center_force[1][i] = 0.0f;
        m->peak_body_force[0][i] = 0.0f;
        m->peak_body_force[1][i] = 0.0f;
    }
    for (i = 0; i < 4; i++) {
        m->lasttp[i] = stree_nodes;
        j = 0;
        do {
            j++;
            m->bodyForce[i][j - 1] = 0.0f;
        } while (j < 3);
    }
}
