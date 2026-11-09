/* The audio options screen's update (MAT3 audioscr.c's updateAudioScreen, validOption, updateButtons): from the
 * front end's options or the pause screen, four lines (effects volume, music volume, song, mono or stereo) on
 * spinning 3D buttons, with a cursor. */
#include "game/game.h"

/* The audio screen's lines (gAudioMenuSelect). */
enum { AUDIO_MENU_SFX = 0, AUDIO_MENU_MUSIC = 1, AUDIO_MENU_SONG = 2, AUDIO_MENU_MONO = 3 };
/* The song line's choices (audio_gSong, gOptions' track): a song of gSongNames, the track's own, none. */
#define AUDIO_SONG_TRACKS 12
#define AUDIO_SONG_LAST 13
/* The song the front end plays for the track's own (song 12) choice. */
#define AUDIO_FRONT_END_SONG 6
/* gGameState's front end states (the screen opened from the menus, not from the pause screen). */
#define GAME_STATE_FRONT_END 0x7C03FFFE

/* An audio screen button (audio_buttonInfo: 0-3 the lines' left buttons, 4-7 their right, 8 the cursor, 9-11
 * the decorations). */
typedef struct {
    s32 type;            /* 0x00 */
    s32 handle;          /* 0x04 */
    f32 ang;             /* 0x08 */
    f32 mat[3][3];       /* 0x0C */
    f32 pos[3];          /* 0x30 */
    u8 pad3C[4];
} Button; /* 0x40 */

VIEW(Button[], audio_buttonInfo);
#define audio_buttonInfo audio_buttonInfo_view

VIEW(f32[], unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

f32 fabsf(f32);
void *memcpy(void *, const void *, u32);
intptr_t MBOX_FindTexture_WORK();
void ZOID_SetTexture(s16 handle, intptr_t tex, s32 c);

/* Whether line i of the screen can be chosen: always (MAT3 hides some lines on other machines). */
s32 audio_validOption(s32 i) {
    s32 v;
    v = 1;
    return v;
}

/* Moves the buttons each frame: the chosen line's pair turns to face the player (a quarter turn in an eighth of a
 * second), the others turn away; the lines slide to their places (at most 180 units a second); the cursor spins
 * and follows the chosen line, faster the farther it has to go. */
void audio_updateButtons(void) {
    s32 i;
    intptr_t name;
    f32 y;
    f32 cur;
    f32 m[3][3];
    f32 k;
    f32 old[3];
    f32 d;
    f32 sel_y;
    s32 vis;
    s16 tex;

    if (audio_buttonsInited == 0)
        return;
    y = 100.0f;
    for (i = 0; i < 4; i++) {
        if (i == gAudioMenuSelect) {
            sel_y = y;
            if (audio_buttonInfo[i].ang < 3.1415927f)
                audio_buttonInfo[i].ang += 12.566371f * gFrameSeconds;
            if (3.1415927f < audio_buttonInfo[i].ang || audio_buttonsJustInited == 1)
                audio_buttonInfo[i].ang = 3.1415927f;
        } else {
            if (0.0f < audio_buttonInfo[i].ang)
                audio_buttonInfo[i].ang -= 12.566371f * gFrameSeconds;
            if (audio_buttonInfo[i].ang < 0.0f || audio_buttonsJustInited == 1)
                audio_buttonInfo[i].ang = 0.0f;
        }
        memcpy(audio_buttonInfo[i].mat, unitmatrix, 36);
        memcpy(old, audio_buttonInfo[i].pos, 12);
        audio_buttonInfo[i].pos[0] = -90.0f;
        audio_buttonInfo[i].pos[1] = y;
        audio_buttonInfo[i].pos[2] = 300.0f;
        if (audio_buttonsJustInited == 0) {
            if (old[1] + 180.0f * gFrameSeconds < audio_buttonInfo[i].pos[1])
                audio_buttonInfo[i].pos[1] = old[1] + 180.0f * gFrameSeconds;
            if (audio_buttonInfo[i].pos[1] < old[1] - 180.0f * gFrameSeconds)
                audio_buttonInfo[i].pos[1] = old[1] - 180.0f * gFrameSeconds;
        }
        audio_buttonInfo[i + 4].ang = audio_buttonInfo[i].ang;
        audio_buttonInfo[i + 4].pos[0] = 150.0f;
        audio_buttonInfo[i + 4].pos[1] = audio_buttonInfo[i].pos[1];
        audio_buttonInfo[i + 4].pos[2] = audio_buttonInfo[i].pos[2];
        WPitchUV(audio_buttonInfo[i].ang - 1.5707964f, audio_buttonInfo[i].mat);
        memcpy(audio_buttonInfo[i + 4].mat, audio_buttonInfo[i].mat, 36);
        if (1.5707964f < audio_buttonInfo[i].ang)
            name = MBOX_FindTexture_WORK("BUTTON_SELECT", &tex, 0, (s8)(gNumModels - 1), 1);
        else
            name = MBOX_FindTexture_WORK("BUTTON", &tex, 0, (s8)(gNumModels - 1), 1);
        ZOID_SetTexture(audio_buttonInfo[i].handle, name, -1);
        ZOID_SetTexture(audio_buttonInfo[i + 4].handle, name, -1);
        if ((vis = audio_validOption(i)) == 0) {
            MBOX_HideObject(audio_buttonInfo[i].handle, 1, 15);
            MBOX_HideObject(audio_buttonInfo[i + 4].handle, 1, 15);
        } else {
            MBOX_ShowObject(audio_buttonInfo[i].handle, 0, 15);
            MBOX_ShowObject(audio_buttonInfo[i + 4].handle, 0, 15);
            y -= 40.0f;
        }
    }
    memcpy(old, audio_buttonInfo[8].pos, 12);
    audio_buttonInfo[8].ang += 6.2831855f * gFrameSeconds;
    if (6.2831855f < audio_buttonInfo[8].ang)
        audio_buttonInfo[8].ang -= 6.2831855f;
    memcpy(audio_buttonInfo[8].mat, unitmatrix, 36);
    audio_buttonInfo[8].pos[0] = -160.0f;
    audio_buttonInfo[8].pos[2] = 225.0f;
    audio_buttonInfo[8].pos[1] = sel_y * 225.0f / 300.0f;
    if (audio_buttonsJustInited == 0) {
        y = audio_buttonInfo[8].pos[1];
        if (audio_cursorSpeed < fabsf(y - old[1]) * 60.0f / 10.0f) {
            audio_cursorSpeed = fabsf(y - old[1]) * 60.0f / 10.0f;
            audio_cursorSpeed = (audio_cursorSpeed < 540.0f * audio_gButtonScale) ? audio_cursorSpeed : 540.0f * audio_gButtonScale;
            audio_cursorSpeed = (180.0f * audio_gButtonScale < audio_cursorSpeed) ? audio_cursorSpeed : 180.0f * audio_gButtonScale;
        }
        if (old[1] + audio_cursorSpeed * gFrameSeconds < y)
            audio_buttonInfo[8].pos[1] = old[1] + audio_cursorSpeed * gFrameSeconds;
        else if (y < old[1] - audio_cursorSpeed * gFrameSeconds)
            audio_buttonInfo[8].pos[1] = old[1] - audio_cursorSpeed * gFrameSeconds;
        else
            audio_cursorSpeed = 180.0f * audio_gButtonScale;
    }
    WRollUV(audio_buttonInfo[8].ang, audio_buttonInfo[8].mat);
    WPitchUV(1.5707964f, audio_buttonInfo[8].mat);
    ScaleUV(audio_buttonInfo[8].mat, audio_buttonInfo[8].mat, 0.4f);
    audio_buttonsJustInited = 0;
}


typedef struct { f32 x, y; } Stick;
typedef struct { volatile s8 b0; u8 pad[15]; } Pad16;
typedef struct { u8 pad0[12]; s8 music; s8 sfx; s8 track; s8 mono; } AudioCfg;

VIEW(Stick[4], gContStick);
#define gContStick gContStick_view

VIEW(AudioCfg, gOptions);
#define gOptions gOptions_view
VIEW(Pad16[], gControllerData);
#define gControllerData gControllerData_view

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);


/* Each frame of the audio screen: reads the controller (the first in the front end, the pausing player's in the
 * pause screen); A or start leaves, B restarts the chosen song, up and down move between the lines, left and right
 * change the line's value (the volumes 0 to 10 with wrap, the song, mono) and play the change at once. Then
 * moves the buttons, notes a pulled controller in the title, and on leaving (or a soft reset) goes back to the
 * options or the pause screen. */
void updateAudioScreen(void) {
    Pad16 *p;
    s32 padB;
    s32 btn;
    f32 stick[2];
    s32 ds;
    s32 dz;

    if (gGameState & GAME_STATE_FRONT_END) {
        stick[0] = gContStick[0].x;
        stick[1] = gContStick[0].y;
        btn = gActiveEdges;
    } else {
        btn = gContEdges[playerInPause];
        stick[0] = gContStick[playerInPause].x;
        stick[1] = gContStick[playerInPause].y;
    }
    if (audio_inited == 0)
        audio_initScreen();
    if (btn & 5) {
        PlayStartABSound(btn);
        exitAudioScreen = 2;
    } else if (btn & 2) {
        switch (gAudioMenuSelect) {
        case AUDIO_MENU_SFX:
            break;
        case AUDIO_MENU_MUSIC:
            break;
        case AUDIO_MENU_SONG:
            AudKillSong();
            AudSetMusicVol(gOptions.music / 10.0f, 0.05f);
            if (audio_gSong < AUDIO_SONG_TRACKS)
                AudPlaySong(audio_gSong, 1);
            else if (audio_gSong == AUDIO_SONG_TRACKS) {
                if (gGameState & GAME_STATE_FRONT_END)
                    AudPlaySong(AUDIO_FRONT_END_SONG, 1);
                else
                    StartGameMusic();
            }
            break;
        }
    } else if (btn & 0xC00) {
        ds = (btn & 0x400) ? -1 : 1;
        PlayUDSound(btn);
        do {
            gAudioMenuSelect += ds;
            if (gAudioMenuSelect > 3)
                gAudioMenuSelect = 0;
            else if (gAudioMenuSelect < 0)
                gAudioMenuSelect = 3;
        } while (!audio_validOption(gAudioMenuSelect));
        if (gAudioMenuSelect == AUDIO_MENU_MUSIC || gAudioMenuSelect == AUDIO_MENU_SONG) {
            AudSetMusicVol(gOptions.music / 10.0f, 0.05f);
            if (audio_gSong < AUDIO_SONG_TRACKS)
                AudPlaySong(audio_gSong, 1);
            else if (audio_gSong == AUDIO_SONG_TRACKS) {
                if (gGameState & GAME_STATE_FRONT_END)
                    AudPlaySong(AUDIO_FRONT_END_SONG, 1);
                else
                    StartGameMusic();
            }
        }
    } else if (btn & 0x3000) {
        dz = (btn & 0x1000) ? -1 : 1;
        PlayLRSound(btn);
        switch (gAudioMenuSelect) {
        case AUDIO_MENU_SFX:
            gOptions.sfx += dz;
            if (gOptions.sfx > 10)
                gOptions.sfx = 0;
            else if (gOptions.sfx < 0)
                gOptions.sfx = 10;
            AudSetFxVol(gOptions.sfx / 10.0f, 0.05f);
            break;
        case AUDIO_MENU_MUSIC:
            gOptions.music += dz;
            if (gOptions.music > 10)
                gOptions.music = 0;
            else if (gOptions.music < 0)
                gOptions.music = 10;
            AudSetMusicVol(gOptions.music / 10.0f, 0.05f);
            break;
        case AUDIO_MENU_SONG:
            AudKillSong();
            do {} while (0);
            audio_gSong += dz;
            if (audio_gSong > AUDIO_SONG_LAST)
                audio_gSong = 0;
            else if (audio_gSong < 0)
                audio_gSong = AUDIO_SONG_LAST;
            gLastSong = audio_gSong;
            gOptions.track = audio_gSong;
            AudSetMusicVol(gOptions.music / 10.0f, 0.05f);
            if (audio_gSong < AUDIO_SONG_TRACKS)
                AudPlaySong(audio_gSong, 1);
            else if (audio_gSong == AUDIO_SONG_TRACKS) {
                if (gGameState & GAME_STATE_FRONT_END)
                    AudPlaySong(AUDIO_FRONT_END_SONG, 1);
                else
                    StartGameMusic();
            }
            break;
        case AUDIO_MENU_MONO:
            gOptions.mono = !gOptions.mono;
            AudMono(gOptions.mono);
            break;
        }
    }
    audio_updateButtons();
    if (gGameState & 0x7C0000) {
        p = &gControllerData[playerInPause];
        if (!p->b0 != audio_gControllerRemoved) {
            audio_gControllerRemoved = !p->b0;
            audio_TitlePane();
        }
    }
    if (gSoftReset) {
        audio_uninitScreen();
        if (gGameState & GAME_STATE_FRONT_END)
            gNewGameState = 4;
        else
            setPauseState(1);
    } else if (exitAudioScreen) {
        audio_uninitScreen();
        if (gGameState & GAME_STATE_FRONT_END)
            gNewGameState = 16;
        else
            setPauseState(1);
    }
}
