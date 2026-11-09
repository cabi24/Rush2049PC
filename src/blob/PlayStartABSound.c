#include "game/game.h"

void PlayStartABSound(s32 arg0)
{
  s32 type;
  type = (arg0 & 1) ? 0x2E : ((arg0 & 2) ? 0x25 : 0x26);
  AudPlayEffect(type, 0, 1, 0U);
}

