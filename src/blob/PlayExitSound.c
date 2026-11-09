/* PlayExitSound: the front end's sound for leaving a screen (MAT3 audio.c). */
#include "game/game.h"

/* Plays the select sound when edges (the menu's button edges) has A, else the back sound. */
void PlayExitSound(s32 edges) {
    s32 fxid;

    if (edges & MENU_EDGE_A) {
        fxid = kSfxMenuSelect;
    } else {
        fxid = kSfxMenuBack;
    }
    AudPlayEffect(fxid, 0, 1, 0U);
}
