/* PlayUDSound: the front end's sound for a move up or down in a menu (MAT3 audio.c). */
#include "game/game.h"

/* Plays the up sound when edges (the menu's button edges) has up, else the down sound. */
void PlayUDSound(s32 edges) {
    s32 fxid;

    if (edges & MENU_EDGE_UP) {
        fxid = kSfxMenuUp;
    } else {
        fxid = kSfxMenuDown;
    }
    AudPlayEffect(fxid, 0, 1, 0U);
}
