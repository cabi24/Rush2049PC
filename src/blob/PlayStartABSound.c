/* PlayStartABSound: the start screen's sound for its buttons (MAT3 audio.c). */
#include "game/game.h"

/* Plays Start's sound when edges (the menu's button edges) has Start, else A's when it has A, else the back sound
 * (B). */
void PlayStartABSound(s32 edges)
{
  s32 fxid;
  fxid = (edges & BUTTON_START) ? SND_MENU_START : ((edges & BUTTON_A) ? SND_MENU_A : SND_MENU_BACK);
  AudPlayEffect(fxid, 0, 1, 0U);
}
