/* accelerations.c: a car's linear and angular accelerations from its forces (MAT3 drivsym.c, accelerations()).
 *
 * A stage of the physics step (sym): after forces and torques sum the forces and moments on the car, this turns
 * them into accelerations that velocities then integrates. */
#include "game/game.h"

/* The model record (tModelData), the fields accelerations reads and writes. */
typedef struct tModelData {
    u8 pad0[0x10];
    f32 f[3];                   /* +0x010 the total force, in the car's frame */
    f32 m[3];                   /* +0x01C the total moment */
    f32 a[3];                   /* +0x028 the acceleration, feet a second squared */
    f32 aa[3];                  /* +0x034 the angular acceleration, radians a second squared */
    u8 pad40[0x5BC - 0x40];
    f32 weight;                 /* +0x5BC the car's weight (a force: mass times gravity) */
} tModelData;

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
