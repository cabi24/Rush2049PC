/* The audio options screen's set-up (MAT3 audioscr.c's initScreen and initButtons): its buttons, blits and the song. */
#include "game/game.h"


void *memcpy(void *,const void *,u32);


/* Opens the audio screen: keeps the globals (SaveGlobals), sizes the menu; in the front end makes the background,
 * the 3D buttons (the lines' pairs, the cursor and three decorations) and the front end's blit, in the pause
 * screen the pause blit. Then, if the music or song line is chosen, restarts the chosen song at the options'
 * music volume. */
void audio_initScreen(void)
{
    s32 u0, u1, u2, u3;

    SaveGlobals();
    audio_calcMenuRect();
    if (gGameState & GS_FRONTEND_MASK) {
        createBG();
        {
        tRGBA buttonColor = {255, 255, 255, 255};
        tTexture *tex;
        unsigned short texIdx;
        s32 buttonIdx;
        for (buttonIdx = 0; buttonIdx < 12; buttonIdx++) {
            if (audio_buttonInfo[buttonIdx].handle == -1) {
                memcpy(audio_buttonInfo[buttonIdx].matrix, unitmatrix, 36);
                audio_buttonInfo[buttonIdx].handle = MBOX_NewObject(
                    MBOX_FindObject_WORK(audio_buttonNames[audio_buttonInfo[buttonIdx].type], 0,
                                       (s8)(gNumModels[0] - 1), 1),
                    &audio_buttonInfo[buttonIdx].matrix, -1,
                    (audio_buttonInfo[buttonIdx].type == 0 ||
                     audio_buttonInfo[buttonIdx].type == 1 ||
                     audio_buttonInfo[buttonIdx].type == 2) ? 0x42000 : 0);
                ZOID_SetColor(audio_buttonInfo[buttonIdx].handle, &buttonColor);
            }
        }
        tex = MBOX_FindTexture_WORK(audio_init_texture_name_1, &texIdx, 0, (s8)(gNumModels[0] - 1), 1);
        ZOID_SetTexture(audio_buttonInfo[8].handle, tex, -1);
        tex = MBOX_FindTexture_WORK(audio_init_texture_name_2, &texIdx, 0, (s8)(gNumModels[0] - 1), 1);
        ZOID_SetTexture(audio_buttonInfo[9].handle, tex, -1);
        memcpy(audio_buttonInfo[9].matrix, unitmatrix, 36);
        WPitchUV(-1.5707964f, audio_buttonInfo[9].matrix);
        audio_buttonInfo[9].position[0] = -75.1f;
        audio_buttonInfo[9].position[1] = 55.2f;
        audio_buttonInfo[9].position[2] = 100.0f;
        ScaleUV(audio_buttonInfo[9].matrix, audio_buttonInfo[9].matrix, 0.5f);
        memcpy(audio_buttonInfo[10].matrix, unitmatrix, 36);
        WPitchUV(-1.5707964f, audio_buttonInfo[10].matrix);
        audio_buttonInfo[10].position[0] = -87.1f;
        audio_buttonInfo[10].position[1] = 55.2f;
        audio_buttonInfo[10].position[2] = 100.0f;
        tex = MBOX_FindTexture_WORK(audio_init_texture_name_3, &texIdx, 0, (s8)(gNumModels[0] - 1), 1);
        ZOID_SetTexture(audio_buttonInfo[11].handle, tex, -1);
        do {} while (0);
        memcpy(audio_buttonInfo[11].matrix, unitmatrix, 36);
        WPitchUV(-1.5707964f, audio_buttonInfo[11].matrix);
        audio_buttonInfo[11].position[0] = 0.0f;
        audio_buttonInfo[11].position[1] = 115.0f;
        audio_buttonInfo[11].position[2] = 200.0f;
        audio_buttonsInited = 1;
        audio_buttonsJustInited = 1;
        ZeroCameras();
}
        audio_blit = NewMultiBlit(0,0,&audio_frontEndBlit,1);
    } else audio_blit = NewMultiBlit(0,0,&audio_pauseBlit,2);
    ClearRushControls();
    exitAudioScreen = 0;
    audio_gSong = gOptions[kOptSong];
    if (gAudioMenuSelect == 1 || gAudioMenuSelect == 2) {
        AudKillSong();
        AudSetMusicVol(gOptions[kOptMusicVolume] / 10.0f, 0.05f);
        if (audio_gSong < AUDIO_SONG_TRACKS) AudPlaySong(audio_gSong,1);
        else if (audio_gSong == AUDIO_SONG_TRACKS) {
            if (gGameState & GS_FRONTEND_MASK) AudPlaySong(AUDIO_FRONT_END_SONG,1);
            else StartGameMusic();
        }
    }
    audio_inited = 1;
}
