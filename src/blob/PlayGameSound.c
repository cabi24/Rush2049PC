/* PlayGameSound: a sound of the race, if the game's sounds are on (MAT3 audio.c). */
#include "game/game.h"

/* Plays effect fxid (AudPlayEffect: subId, replace, priority) and returns its fx tag handle; -1 when the game's
 * sounds are off (EnableGameSounds) or fxid is -1. */
s32 PlayGameSound(s32 fxid, s32 subId, s32 replace, unsigned char priority)
{
  if (gGameSoundsEnabled == 0)
  {
    return -1;
  }
  if (fxid == (-1))
  {
    return -1;
  }
  return AudPlayEffect(fxid, subId, replace, priority);
}
