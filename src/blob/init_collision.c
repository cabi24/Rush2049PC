/* init_collision.c: clear a car's collision state (MAT3 collision.c, init_collision()).
 *
 * Part of a car's set-up (syminit), at the race start and after each respawn. */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    unsigned char pad0[0xC4];
    f32 bodyForce[4][3];            /* 0x0C4 the force on each body corner this step */
    unsigned char pad0F4[0x124 - 0xF4];
    f32 centerForce[3];             /* 0x124 the force on the body's centre this step */
    unsigned char pad130[0x148 - 0x130];
    f32 peak_body_force[2][3];      /* 0x148 */
    f32 peak_center_force[2][3];    /* 0x160 */
    unsigned char pad178[0x3A0 - 0x178];
    int lasttp[4];                  /* 0x3A0 each tire's last road-tree node: a 32-bit window address */
} tModelData;
VIEW(intptr_t, stree_nodes);
#define stree_nodes stree_nodes_view
/* The model's road-tree nodes keep 32-bit N64 addresses (CollideCarBody's enum n64p_QNode): N64A(p) is a window
 * pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)

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
        m->lasttp[i] = N64A(stree_nodes);
        j = 0;
        do {
            j++;
            m->bodyForce[i][j - 1] = 0.0f;
        } while (j < 3);
    }
}
