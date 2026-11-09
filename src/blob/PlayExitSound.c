/* PlayExitSound: the front end's sound for leaving a screen (MAT3 audio.c). */
#include "game/game.h"

/* Plays Start's sound when edges (the menu's button edges) has Start, else the back sound. */
void PlayExitSound(s32 edges) {
    s32 fxid;

    if (edges & BUTTON_START) {
        fxid = SND_MENU_START;
    } else {
        fxid = SND_MENU_BACK;
    }
    AudPlayEffect(fxid, 0, 1, 0U);
}
