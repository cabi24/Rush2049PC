/* End_MDrive.c: stop a car's simulation at the end of its race (MAT3 mdrive.c, End_MDrive()). */
#include "game/game.h"

/* The model record (tModelData), the fields End_MDrive clears. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0x398];
    streetyp *lasttp[4];
    u8 pad3B0[0x5C];
    const s16 *torquecurve;
    u8 pad410[0x238];
    surftyp *movingSurface;
    u8 pad64C[0xD0];
    s16 modelrun;               /* +0x71C fastin.modelrun: the model thread steps this car */
    u8 pad71E[0x7F8 - 0x71E];
    f32 skidIntensity[4];       /* +0x7F8 each tire's skid sound level */
} tModelData;

/* Stop car `slot`: the model thread no longer steps it (modelrun 0), its engine and tire sounds stop
 * (StopCarSounds) and its skids fall silent. */
void End_MDrive(s16 slot)
{
  tModelData *m_copy;   /* m2c's copy of m, kept: the code stores it */
  tModelData *m;
  m = PC_MODEL(slot);
  if (1)
  {
    m->modelrun = 0;
    m_copy = m;
    StopCarSounds(m);
    m->skidIntensity[1] = 0.0f;
    m->skidIntensity[2] = 0.0f;
  }
  m->skidIntensity[3] = 0.0f;
  m->skidIntensity[0] = 0.0f;
}
