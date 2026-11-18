/* dead_reckon_posuv.c: one car's dead-reckoning snapshot (MAT3 rekon.c, dead_reckon_posuv()).
 *
 * The snapshot (tModelData.reckon) is what the rest of the game sees of a car between model steps: the
 * orientation, the speed shown, the suspension, and the acceleration, velocity and position as of the last base
 * update (update_reckon_base). game_reckon_all then moves the position on to the frame's time. */
#include "game/game.h"

#define mveccopy(a,r) {r[0]=a[0]; r[1]=a[1]; r[2]=a[2];}

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0x2E4];
    f32 uvs[9];                 /* 0x2EC the orientation */
    u8 pad310[0x3A0 - 0x310];
    streetyp *lasttp[4];
    u8 pad3B0[0x40C - 0x3B0];
    const s16 *torquecurve;
    u8 pad410[0x430 - 0x410];
    Tire tires[4];              /* 0x430 */
    u8 pad5A0[0x5CC - 0x5A0];
    f32 suscomp[4];             /* 0x5CC */
    u8 pad5DC[0x5EC - 0x5DC];
    f32 airdist[4];             /* 0x5EC */
    u8 pad5FC[0x648 - 0x5FC];
    surftyp *movingSurface;
    u8 pad64C[0x734 - 0x64C];
    tReckon reckon;             /* 0x734 */
} tModelData;

/* Take model `m`'s snapshot: its orientation; the speed shown (rwmph), the rear wheels' average rim speed
 * (angvel x radius, feet a second) times 2.72727275, which is 60/88 (feet a second to mph) times 4; each tire's
 * suspension compression and height; and the base acceleration, velocity and position. */
void dead_reckon_posuv(tModelData *m)
{
    CopyUV(m->uvs, m->reckon.uvs);
    m->reckon.rwmph = (s16)(int)(((m->tires[3].tradius * m->tires[3].angvel + m->tires[2].angvel * m->tires[2].tradius) * 0.5f) * 2.72727275f);
    {
        int i;
        for (i = 0; i < 4; i++) {
            m->reckon.suscomp[i] = m->suscomp[i];
            m->reckon.airdist[i] = m->airdist[i];
        }
    }
    mveccopy((&m->reckon.base_rwa[0]), (&m->reckon.rwa[0]));
    mveccopy((&m->reckon.base_rwv[0]), (&m->reckon.rwv[0]));
    mveccopy((&m->reckon.base_rwr[0]), (&m->reckon.rwr[0]));

}
