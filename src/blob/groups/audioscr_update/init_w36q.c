#include "game/game.h"

typedef struct TexDef TexDef; typedef struct Blit Blit; typedef struct MultiBlit MultiBlit;
typedef struct { s32 type, handle; f32 ang; f32 mat[3][3]; f32 pos[3]; u8 pad3C[4]; } Button;

typedef union Color { u8 c[4]; u32 rgba; } Color;
typedef struct { u8 pad0[12]; s8 music; s8 sfx; s8 track; s8 mono; } AudioCfg;
VIEW(AudioCfg, gOptions);
#define gOptions gOptions_view
VIEW(Button[12], audio_buttonInfo);
#define audio_buttonInfo audio_buttonInfo_view
VIEW(u8, gNumModels);
#define gNumModels gNumModels_view
VIEW(f32[3][3], unitmatrix);
#define unitmatrix unitmatrix_view

void *memcpy(void *,const void *,u32);

TexDef *MBOX_FindTexture_WORK(char *,u16 *,s8,s8,s32);
void ZOID_SetTexture(s16,TexDef *,s32);

Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);

s32 osSendMesg(OSMesgQueue *, void *, s32);
/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The stop of the song (command 1) that di_post_type1 posts
 * is then dropped: with no message the lock is given back and nothing is sent, and a message the full queue
 * refuses is given back (its in-use byte, +3). Nothing else is made here; the song plays on to the next stop. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){audio_unlock();return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)(m)->used=0

void audio_initScreen(void)
{
    s32 u0, u1, u2, u3;

    SaveGlobals();
    audio_calcMenuRect();
    if (gGameState & 0x7C03FFFE) {
        createBG();
        {
        Color buttonColor = {255, 255, 255, 255};
        TexDef *tex;
        unsigned short texIdx;
        s32 buttonIdx;
        for (buttonIdx = 0; buttonIdx < 12; buttonIdx++) {
            if (audio_buttonInfo[buttonIdx].handle == -1) {
                memcpy(audio_buttonInfo[buttonIdx].mat, unitmatrix, 36);
                audio_buttonInfo[buttonIdx].handle = MBOX_NewObject(
                    MBOX_FindObject_WORK(audio_buttonNames[audio_buttonInfo[buttonIdx].type], 0,
                                       (s8)(gNumModels - 1), 1),
                    &audio_buttonInfo[buttonIdx].mat, -1,
                    (audio_buttonInfo[buttonIdx].type == 0 ||
                     audio_buttonInfo[buttonIdx].type == 1 ||
                     audio_buttonInfo[buttonIdx].type == 2) ? 0x42000 : 0);
                ZOID_SetColor(audio_buttonInfo[buttonIdx].handle, &buttonColor);
            }
        }
        tex = MBOX_FindTexture_WORK(audio_init_texture_name_1, &texIdx, 0, (s8)(gNumModels - 1), 1);
        ZOID_SetTexture(audio_buttonInfo[8].handle, tex, -1);
        tex = MBOX_FindTexture_WORK(audio_init_texture_name_2, &texIdx, 0, (s8)(gNumModels - 1), 1);
        ZOID_SetTexture(audio_buttonInfo[9].handle, tex, -1);
        memcpy(audio_buttonInfo[9].mat, unitmatrix, 36);
        WPitchUV(-1.5707964f, audio_buttonInfo[9].mat);
        audio_buttonInfo[9].pos[0] = -75.1f;
        audio_buttonInfo[9].pos[1] = 55.2f;
        audio_buttonInfo[9].pos[2] = 100.0f;
        ScaleUV(audio_buttonInfo[9].mat, audio_buttonInfo[9].mat, 0.5f);
        memcpy(audio_buttonInfo[10].mat, unitmatrix, 36);
        WPitchUV(-1.5707964f, audio_buttonInfo[10].mat);
        audio_buttonInfo[10].pos[0] = -87.1f;
        audio_buttonInfo[10].pos[1] = 55.2f;
        audio_buttonInfo[10].pos[2] = 100.0f;
        tex = MBOX_FindTexture_WORK(audio_init_texture_name_3, &texIdx, 0, (s8)(gNumModels - 1), 1);
        ZOID_SetTexture(audio_buttonInfo[11].handle, tex, -1);
        do {} while (0);
        memcpy(audio_buttonInfo[11].mat, unitmatrix, 36);
        WPitchUV(-1.5707964f, audio_buttonInfo[11].mat);
        audio_buttonInfo[11].pos[0] = 0.0f;
        audio_buttonInfo[11].pos[1] = 115.0f;
        audio_buttonInfo[11].pos[2] = 200.0f;
        audio_buttonsInited = 1;
        audio_buttonsJustInited = 1;
        ZeroCameras();
}
        audio_blit = NewMultiBlit(0,0,&audio_frontEndBlit,1);
    } else audio_blit = NewMultiBlit(0,0,&audio_pauseBlit,2);
    ClearRushControls();
    exitAudioScreen = 0;
    audio_gSong = gOptions.track;
    if (gAudioMenuSelect == 1 || gAudioMenuSelect == 2) {
        AudKillSong();
        AudSetMusicVol(gOptions.music / 10.0f, 0.05f);
        if (audio_gSong < 12) AudPlaySong(audio_gSong,1);
        else if (audio_gSong == 12) {
            if (gGameState & 0x7C03FFFE) AudPlaySong(6,1);
            else StartGameMusic();
        }
    }
    audio_inited = 1;
}


