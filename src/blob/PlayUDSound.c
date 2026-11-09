/* PlayUDSound: the front end's sound for a move up or down in a menu (MAT3 audio.c). */
#include "game/game.h"

/* Plays the up sound when edges (the menu's button edges) has up, else the down sound. */
void PlayUDSound(s32 edges) {
    s32 fxid;

    if (edges & BUTTON_UP) {
        fxid = SND_MENU_UP;
    } else {
        fxid = SND_MENU_DOWN;
    }
    AudPlayEffect(fxid, 0, 1, 0U);
}
