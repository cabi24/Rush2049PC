/* PlayStartABSound: the start screen's sound for its buttons (MAT3 audio.c). */
#include "game/game.h"

/* Plays the select sound for A, the start screen's B sound for B, else the back sound (edges: the menu's button
 * edges). */
void PlayStartABSound(s32 edges)
{
  s32 fxid;
  fxid = (edges & MENU_EDGE_A) ? kSfxMenuSelect : ((edges & MENU_EDGE_B) ? kSfxMenuStartB : kSfxMenuBack);
  AudPlayEffect(fxid, 0, 1, 0U);
}
