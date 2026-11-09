#include "game/game.h"

/* PlayGameSound: sound-cue wrapper (returns -1 when sound is off or the cue is -1, else
 * AudPlayEffect). Direct-return spelling, replacing the permuter's `new_var`
 * local. Same 21 words standalone (-O2 and -O3) and EQUAL in the whole-program unit with 0 locked bodies
 * differing; the only locked inliner (CheckCPs, group car_checkpoints) stays EQUAL.
 * Inlined copies of this form cost less frame, which audio_doppler_full and PassedCP need. */
s32 PlayGameSound(s32 arg0, s32 arg1, s32 arg2, unsigned char arg3)
{
  if (gGameSoundsEnabled == 0)
  {
    return -1;
  }
  if (arg0 == (-1))
  {
    return -1;
  }
  return AudPlayEffect(arg0, arg1, arg2, arg3);
}
