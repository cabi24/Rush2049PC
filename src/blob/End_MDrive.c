/* End_MDrive.c: stop a car's simulation at the end of its race (MAT3 mdrive.c, End_MDrive()). */
#include "game/game.h"

/* Stop car `slot`: the model thread no longer steps it (modelrun 0), its engine and tire sounds stop
 * (StopCarSounds) and its skids fall silent. */
void End_MDrive(s16 slot)
{
  tModelData *m_copy;   /* m2c's copy of m, kept: the code stores it */
  tModelData *m;
  m = &pc_models[slot];
  if (1)
  {
    m->fastin.modelrun = 0;
    m_copy = m;
    StopCarSounds(m);
    m->skidIntensity[1] = 0.0f;
    m->skidIntensity[2] = 0.0f;
  }
  m->skidIntensity[3] = 0.0f;
  m->skidIntensity[0] = 0.0f;
}
