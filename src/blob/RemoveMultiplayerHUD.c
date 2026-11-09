#include "game/game.h"

extern void RemoveBlit(intptr_t sound_id);

VIEW(intptr_t, multiPlayerBlit);
#define multiPlayerBlit multiPlayerBlit_view
void RemoveMultiplayerHUD(void)
{
  intptr_t handle;

  handle = multiPlayerBlit;
  if (handle != 0)
  {
    RemoveBlit(handle);
    multiPlayerBlit = 0;
  }
}


