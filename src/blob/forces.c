/* forces.c: all the forces on a car for one step (MAT3 drivsym.c, forces()). */
#include "game/game.h"

/* The forces on model m: forces1 (the drivetrain, the body's contacts, the resistances) then forces2 (the tires, the
 * gravity). */
void forces(tModelData *m) {
    forces1(m);
    forces2(m);
}
