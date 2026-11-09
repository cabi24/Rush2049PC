/* CalcMass.c: a car's weight from its parts (MAT3 initiali.c, CalcMass()).
 *
 * One of the set-up helpers carsetup calls to fill the model record. */
#include "game/game.h"

/* The model record (tModelData), the car's parts it reads. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 body_type;       /* +0x008 the car, 0..12 */
    s8 trans;           /* +0x009 */
    s8 autotrans;       /* +0x00A */
    s8 handling;        /* +0x00B the handling (tune) choice */
    s8 engine_type;     /* +0x00C */
    s8 tire_type;       /* +0x00D */
    s8 frame;           /* +0x00E the frame (durability) choice */
} tModelData;

/* The weight of model m: the body's gCarMass, plus mass_unit (MAT3 31.0559) for each step of its frame's durability
 * above mass_durability_base and for each step of the "massive mass" cheat (gMassiveMass), all scaled by its tune's
 * weight_scale (the second word of gTune[handling]). */
f32 CalcMass(tModelData *m)
{
  f32 durability = 0.0f;
  if (mass_unit) {}   /* no effect: the N64 compiler's code reads mass_unit here */
  durability = carDurability[m->frame] - mass_durability_base;
  return (gCarMass[m->body_type] + durability * mass_unit + ((f32) gMassiveMass) * mass_unit) * gTune[m->handling].weight_scale;
}
