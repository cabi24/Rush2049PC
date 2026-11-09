/* regular.c: one ordinary step of a car's simulation (MAT3 drivsym.c, regular()).
 *
 * The physics step (sym) runs it for a car that is driving (not respawning, not crashed out). */
#include "game/game.h"

/* The model record (tModelData): the stages read and write it. */
typedef struct tModelData tModelData;

/* Model m's step, stage by stage: the road under the tires and the body (road), the forces (forces1, forces2), the
 * arcade yaw help (antispin), the moments (torques), the accelerations, then the integration of the velocities and
 * the positions. */
void regular(tModelData *m) {
    road(m);
    forces1(m);
    forces2(m);
    antispin(m);
    torques(m);
    accelerations(m);
    velocities(m);
    positions(m);
}
