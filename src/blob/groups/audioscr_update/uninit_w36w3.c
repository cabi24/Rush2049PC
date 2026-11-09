/* The audio options screen's close (MAT3 audioscr.c's uninitScreen and uninitButtons). */
#include "game/game.h"

typedef struct {
    s32 type;            /* 0x00 */
    s32 handle;          /* 0x04 */
    u8 pad08[0x38];
} UButton; /* 0x40 */

VIEW(intptr_t, audio_blit);
#define audio_blit audio_blit_view

VIEW(UButton[12], audio_buttonInfo);
#define audio_buttonInfo audio_buttonInfo_view

s32 osSendMesg(OSMesgQueue *, void *, s32);


/* Closes the audio screen: stops the song, and if the screen was set up removes its buttons, its panes (in the
 * pause screen), its blit and the background. */
void audio_uninitScreen(void)
{
    AudKillSong();
    if (audio_inited) {
        us_uninitButtons();
        if (!(gGameState & GS_FRONTEND_MASK)) {
            RemovePanes(0);
            audio_gTitlePane = 0;
        }
        if (audio_blit) {
            RemoveBlit((tBlit *) audio_blit);
            audio_blit = 0;
        }
        removeBG();
        audio_panesCreated = 0;
        audio_inited = 0;
    }
}

/* Removes the screen's 3D buttons. */
void us_uninitButtons(void)
{
    s32 buttonIdx;

    for (buttonIdx = 0; buttonIdx < 12; buttonIdx++) {
        if (audio_buttonInfo[buttonIdx].handle != -1) {
            RemoveObjectTree(audio_buttonInfo[buttonIdx].handle, 0, 0);
            audio_buttonInfo[buttonIdx].handle = -1;
        }
    }
    audio_buttonsInited = 0;
}
