/* steer_constants.c: a car's steering constants (MAT3 initiali.c, steer_constants()). Part of the car set-up
 * (carsetup). */
#include "game/game.h"

/* The car's physics descriptor (MAT3 _tCar), the fields steer_constants reads. */
typedef struct tCar {
    u8 pad0[0x50];
    f32 steerratio;             /* +0x50 the steering ratio: steering-wheel angle over the road wheels' */
    f32 swtpg;                  /* +0x54 */
    u8 pad58[0x90 - 0x58];
    f32 tireR2z;                /* +0x90 tireR[2][2]: tire 2's (rear) position along the car */
} tCar;
/* The model record (tModelData), the fields steer_constants reads and writes. Its descriptor pointer is a u32 window
 * address: the record keeps its N64 layout (docs/port/CARS.md 4). */
typedef struct tModelData {
    u32 car;                    /* +0x000 the physics descriptor (tCar) */
    u8 pad4[0x3C4 - 0x4];
    f32 steergain;              /* +0x3C4 the road wheels' angle at full lock, radians */
    f32 trail;                  /* +0x3C8 */
    u8 pad3CC[0x5BC - 0x3CC];
    f32 weight;                 /* +0x5BC */
    u8 pad5C0[0x5C8 - 0x5C0];
    f32 wheelbase;              /* +0x5C8 */
} tModelData;

/* Model m's steering gain, the road wheels' angle at full lock: the steering wheel's 540 degrees (one and a half
 * turns) over the descriptor's steering ratio, in radians (steer_deg_to_rad, MAT3 0.017453292); and its trail
 * (MAT3 trail): swtpg times the steering ratio over the weight times tire 2's position over the wheelbase. */
void steer_constants(tModelData *m)
{
  tCar *car;
  car = (tCar *) PC_N64P(m->car);
  m->steergain = (540.0f / car->steerratio) * steer_deg_to_rad;
  m->trail = (car->swtpg * car->steerratio) / (m->weight * (car->tireR2z / m->wheelbase));
}
