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

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
  _tCar *car;
  Tune44 *tune;
  u8 pad008[0x38];
  f32 v[3];                                     /* +0x040 the velocity in the car's frame, feet a second */
  u8 pad04C[0x94 - 0x4C];
  f32 tireV[4][3];                              /* +0x094 each tire's velocity in the car's frame */
  u8 pad0C4[0x3A0 - 0x0C4];
  streetyp *lasttp[4];
  u8 pad3B0[0x3F4 - 0x3B0];
  s16 gear;                                     /* +0x3F4 */
  s16 commandgear;                              /* +0x3F6 */
  u8 pad3F8[0x40C - 0x3F8];
  const s16 *torquecurve;
  u8 pad410[0x430 - 0x410];
  tModelTire tires[4];                          /* +0x430 */
  u8 pad5A0[0x648 - 0x5A0];
  surftyp *movingSurface;
  u8 pad64C[0x660 - 0x64C];
  f32 resurrect_pos[3];                         /* +0x660 resurrect.pos: where the car respawns */
  u8 pad66C[0x678 - 0x66C];
  f32 resurrect_uvs[3][3];                      /* +0x678 resurrect.uvs: its orientation there */
  u8 pad69C[0x6BC - 0x69C];
  s32 resurrect_velocity;                       /* +0x6BC resurrect.velocity: its speed, feet a second */
  u8 pad6C0[0x6C4 - 0x6C0];
  s16 resurrect_moving_state;                   /* +0x6C4 resurrect.moving_state: RES_IDLE, RES_START, RES_DONE */
  u8 pad6C6[0x6D4 - 0x6C6];
  f32 initin_pos[3];                            /* +0x6D4 initin.pos: where syminit starts the car */
  f32 initin_uvs[3][3];                         /* +0x6E0 initin.uvs: and its orientation */
  u8 pad704[0x71C - 0x704];
  s16 modelrun;                                 /* +0x71C fastin.modelrun: the simulation runs this car */
  u8 pad71E[0x730 - 0x71E];
  s8 mainin_gear;                               /* +0x730 mainin.gear: the gear the controls ask for */
  u8 pad731[0x7C6 - 0x731];
  s16 slot;                                     /* +0x7C6 */
} tModelData;

/* If model `m`'s respawn flight is over (RES_DONE): stop simulating it, mark it no longer respawning (RES_IDLE),
 * start its simulation again at the respawn point and orientation (syminit), in second gear if it respawns at 44
 * feet a second (30 mph) or more, else first, with the body and every tire moving forward at the respawn speed
 * and the wheels turning to match, then let the simulation run it again. */
void FinishResurrecting(tModelData *m)
{
    s16 i;
    s16 slot;
    if (m->resurrect_moving_state != RES_DONE) {
        return;
    }
    /* Empty nested tests: the N64 code has three of them here, a debug check whose body the retail build left out.
     * What they tested can't be recovered; they read car 0's modelrun only so that the code keeps their shape. */
    if (PC_MODEL_MODELRUN(0)) {
        if (PC_MODEL_MODELRUN(0)) {
            if (PC_MODEL_MODELRUN(0)) {
            }
        }
    }
    m->modelrun = 0;
    m->resurrect_moving_state = RES_IDLE;
    m->initin_pos[0] = m->resurrect_pos[0];
    m->initin_pos[1] = m->resurrect_pos[1];
    m->initin_pos[2] = m->resurrect_pos[2];
    CopyUV(m->resurrect_uvs, m->initin_uvs);
    syminit(m);
    if ((f32)m->resurrect_velocity >= 44.0f) {
        m->mainin_gear = 2;
        m->commandgear = 2;
        m->gear = 2;
    } else {
        m->mainin_gear = 1;
        m->commandgear = 1;
        m->gear = 1;
    }
    m->v[2] = m->resurrect_velocity;
    i = 0;
    do {
        m->tireV[i][2] = m->resurrect_velocity;
        m->tires[i].angvel = m->tireV[i][2] / m->tires[i].tradius;
        i++;
    } while (i < 4);
    slot = m->slot;
    PC_MODEL_MODELRUN(slot) = 1;
}
