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

void RemoveBlit(intptr_t sound_id);

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The stop of the song (command 1) that us_AudKillSong posts
 * is then dropped: with no message the lock is given back and nothing is sent, and a message the full queue
 * refuses is given back (its in-use byte, +3). Nothing else is made here; the song plays on to the next stop. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){audio_unlock();return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)(m)->used=0

void audio_uninitScreen(void)
{
    AudKillSong();
    if (audio_inited) {
        us_uninitButtons();
        if (!(gGameState & 0x7C03FFFE)) {
            RemovePanes(0);
            audio_gTitlePane = 0;
        }
        if (audio_blit) {
            RemoveBlit(audio_blit);
            audio_blit = 0;
        }
        removeBG();
        audio_panesCreated = 0;
        audio_inited = 0;
    }
}

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
