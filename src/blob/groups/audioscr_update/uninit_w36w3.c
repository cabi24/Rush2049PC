/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * audio_uninitScreen (0x800D6698, 296 B; historical label) = MAT3 uninitScreen@audioscr.c (GC 0x.; N64 form),
 * . Internal (private IPA: s0-s3 unsaved); its only caller is updateAudioScreen = updateAudioScreen (two
 * sites, both in this group). AudKillSong() (N64: under the slot-queue lock audio_lock/audio_unlock, _GetCmd =
 * GetCmd, command 1, then _ProcessCommand = osSendMesg to the audio queue audio_command_queue), then if the screen is
 * inited (audio_inited): uninitButtons() (12 buttons audio_buttonInfo, 64 B each: ZOID_RemoveObject = RemoveObjectTree
 * (objIdx, 0, 0) for live objects, objIdx = -1; buttonsInited audio_buttonsInited = 0), outside the game state
 * (gGameState & 0x7C03FFFE == 0) RemovePanes(0) = RemovePanes(0) and gTitlePane audio_gTitlePane = 0, the blit
 * audio_blit removed (RemoveBlit), removeBG = removeBG(), panesCreated audio_panesCreated = 0,
 * inited = 0.
 * uninitButtons is defined AFTER the caller, as in the GC file
 * (audioscr.c line order): as1 breaks ready-list ties by source line, and retail schedules the gGameState load
 * (caller line) before the inlined buttonsInited store (2 words otherwise).
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef int s32; typedef unsigned int u32; typedef float f32;
typedef struct OSMesgQueue OSMesgQueue;
typedef struct Msg { s16 id; s8 type, used; u8 payload[20]; } Msg;
typedef struct {
    s32 type;            /* 0x00 */
    s32 handle;          /* 0x04 */
    u8 pad08[0x38];
} UButton; /* 0x40 */
extern s32 audio_command_queue;
extern s8 audio_inited;           /* inited */
extern s8 audio_panesCreated;           /* panesCreated */
extern intptr_t audio_blit;          /* blit */
extern intptr_t audio_gTitlePane;          /* gTitlePane */
extern s32 audio_buttonsInited;          /* buttonsInited */
extern UButton audio_buttonInfo[12];  /* buttonInfo */
extern u32 gGameState;
Msg *GetCmd(void);
void audio_lock(void);
void audio_unlock(void);
s32 osSendMesg(OSMesgQueue *, void *, s32);
void RemoveObjectTree(s16 idx, s32 freeChildren, s32 freeSiblings);
void RemovePanes(s32 arg0);
void RemoveBlit(intptr_t sound_id);
void removeBG(void);

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The stop of the song (command 1) that us_AudKillSong posts
 * is then dropped: with no message the lock is given back and nothing is sent, and a message the full queue
 * refuses is given back (its in-use byte, +3). Nothing else is made here; the song plays on to the next stop. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){audio_unlock();return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)(m)->used=0


void us_uninitButtons(void);

void AudKillSong(void);
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
