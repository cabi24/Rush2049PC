/* PlayLRSound: the front end's sound for a move left or right in a menu (MAT3 audio.c). */
#include "game/game.h"

/* Plays the left sound when edges (the menu's button edges) has left, else the right sound. */
void PlayLRSound(s32 edges) {
    s32 fxid;

    if (edges & MENU_EDGE_LEFT) {
        fxid = kSfxMenuLeft;
    } else {
        fxid = kSfxMenuRight;
    }
    AudPlayEffect(fxid, 0, 1, 0U);
}
