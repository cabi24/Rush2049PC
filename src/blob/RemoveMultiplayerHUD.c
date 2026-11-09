/* RemoveMultiplayerHUD: remove the split screen's HUD (MAT3 hud.c). */
#include "game/game.h"

extern void RemoveBlit(intptr_t blit);

VIEW(intptr_t, multiPlayerBlit);
#define multiPlayerBlit multiPlayerBlit_view
/* Removes the multiplayer HUD's blits (multiPlayerBlit), if there are any. */
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


