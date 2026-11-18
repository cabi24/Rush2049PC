/* FinishResurrecting.c: the end of a respawn's flight (MAT3 resurrect.c, FinishResurrecting()).
 *
 * Once UpdateResurrectingCar has flown the car to its respawn point (moving_state RES_DONE), this puts the
 * simulation back on the ground there: the physics state starts again at the point, in the gear and with the wheel
 * speeds for the respawn's speed, and the car runs again. */
#include "game/game.h"

/* A tire's state (MAT3's anon_tradius_springK_5c, 0x5C bytes), the fields used here. */
typedef struct tModelTire {
  f32 tradius;                                  /* +0x00 the tire's radius, feet */
  u8 pad04[0x48 - 0x04];
  f32 angvel;                                   /* +0x48 its angular speed, radians a second */
  u8 pad4C[0x5C - 0x4C];
} tModelTire;

/* If model `m`'s respawn flight is over (RES_DONE): stop simulating it, mark it no longer respawning (RES_IDLE),
 * start its simulation again at the respawn point and orientation (syminit), in second gear if it respawns at 44
 * feet a second (30 mph) or more, else first, with the body and every tire moving forward at the respawn speed
 * and the wheels turning to match, then let the simulation run it again. */
void FinishResurrecting(tModelData *m)
{
    s16 i;
    s16 slot;
    if (m->resurrect.moving_state != RES_DONE) {
        return;
    }
    /* Empty nested tests: the N64 code has three of them here, a debug check whose body the retail build left out.
     * What they tested can't be recovered; they read car 0's modelrun only so that the code keeps their shape. */
    if (pc_models[0].fastin.modelrun) {
        if (pc_models[0].fastin.modelrun) {
            if (pc_models[0].fastin.modelrun) {
            }
        }
    }
    m->fastin.modelrun = 0;
    m->resurrect.moving_state = RES_IDLE;
    m->initin.pos[0] = m->resurrect.pos[0];
    m->initin.pos[1] = m->resurrect.pos[1];
    m->initin.pos[2] = m->resurrect.pos[2];
    CopyUV(m->resurrect.uvs, m->initin.uvs);
    syminit(m);
    if ((f32)m->resurrect.velocity >= 44.0f) {
        m->mainin.gear = 2;
        m->commandgear = 2;
        m->gear = 2;
    } else {
        m->mainin.gear = 1;
        m->commandgear = 1;
        m->gear = 1;
    }
    m->v[2] = m->resurrect.velocity;
    i = 0;
    do {
        m->tireV[i][2] = m->resurrect.velocity;
        m->tires[i].angvel = m->tireV[i][2] / m->tires[i].tradius;
        i++;
    } while (i < 4);
    slot = m->slot;
    pc_models[slot].fastin.modelrun = 1;
}
