/* RemoveMultiplayerHUD: remove the split screen's HUD (MAT3 hud.c). */
#include "game/game.h"

VIEW(intptr_t, multiPlayerBlit);
#define multiPlayerBlit multiPlayerBlit_view
/* Removes the multiplayer HUD's blits (multiPlayerBlit), if there are any. */
void RemoveMultiplayerHUD(void)
{
  tBlit *handle;

  handle = (tBlit *) multiPlayerBlit;
  if (handle != 0)
  {
    RemoveBlit(handle);
    multiPlayerBlit = 0;
  }
}


