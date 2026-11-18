/* accelerations.c: a car's linear and angular accelerations from its forces (MAT3 drivsym.c, accelerations()).
 *
 * A stage of the physics step (sym): after forces and torques sum the forces and moments on the car, this turns
 * them into accelerations that velocities then integrates. */
#include "game/game.h"

/* a = f / mass, as f times gravity over weight (gGravity, feet a second squared), and the angular acceleration from
 * the moment (calcaa). */
void accelerations(tModelData *m) {
    f32 k;

    k = gGravity / m->weight;
    m->a[0] = m->f[0] * k;
    m->a[1] = m->f[1] * k;
    m->a[2] = m->f[2] * k;
    calcaa(m, m->m, m->aa);
}
