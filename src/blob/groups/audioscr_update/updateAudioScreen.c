/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* group audio_options_menu: audio_validOption (stub given a body) + audio_updateButtons (1260 B) + updateAudioScreen (1804 B),
 * all three EQUAL in the whole-program unit with their real callers (no stand-ins, no implicit int, no false prototype).
 *   audio_validOption  (2 w)   deleted helper "row selectable?" (`v = 1; return v;`), umerge-inlined into updateAudioScreen
 *                          (the row-wrap do-while: retail's unfolded `li v0,1; beqzl v0`) and audio_updateButtons
 *                          (`if ((vis = audio_validOption(i)) == 0)`, the sibling video_updateButtons's visibility idiom).
 *                          Its retail remains are the caller-less locked stub at 0x800D690C.
 *   audio_updateButtons  (315 w) audio-options 3D buttons update (sibling of the locked pause-menu video_updateButtons,
 *                          updateVideoScreen group): 4 rows, selected one spun to pi, slider cursor audio_buttonInfo[8].
 *   updateAudioScreen  (451 w) N64 audio-options screen input handler (no arcade ancestor): rows 0 sfx volume
 *                          (gOptions.sfx), 1 music volume (.music), 2 music track (audio_gSong 0..13, 12 = random via
 *                          StartGameMusicInternal(0), 13 = off), 3 stereo/mono (.mono, AudMono). A/B/Start leave
 *                          (PlayStartABSound, exitAudioScreen = 2); A on the track row restarts the track; left/right
 *                          move the row (gAudioMenuSelect, wrap 0..3); up/down change the value; AudKillSong stops the song
 *                          before a new track (MAT3 updateAudioScreen does so; the N64 inlined it twice).
 * Levers (each measured in the unit):
 *  - every AudSetMasterVol call is the locked kept wrapper AudSetMusicVol/AudSetFxVol and StartGameMusicInternal(0) is
 *    the locked StartGameMusic(), all umerge-inlined (8-byte inline areas = retail frame 256).
 *  - audio_updateButtons compares `i == gAudioMenuSelect` directly; s7 = &audio_buttonsJustInited and s8 = 1 come from the inlined
 * audio_validOption visibility test (the gAudioMenuSelect getter was a stand-).
 * Disclosed shaping:
 *  1. updateAudioScreen: one compiled-out block end `do {} while (0);` right after the second AudKillSong() on the track row. Traced:
 *     without it the post block (GetCmd's precoloured v0 result) shares a uopt block with the audio_gSong
 *     address web, so v0 is forbidden to that web and it takes a1 at all four track sites (15 words).
 *  2. Unused locals that are the exact frame residual:
 *     updateAudioScreen `padB` (retail 256, without it 248; 1 slot);
 *     audio_updateButtons `m[3][3]`, `k` (retail 144, without them 104; 10 slots), `cur` and `d` (1 slot each: removal
 *     moves old/sel_y/tex off retail's homes 76/68/62; the sibling video_updateButtons has the same `cur`/`d`).
 *  3. Helper identities: audio_validOption = constant-1 row predicate (hypothesis supported by the stub position, the
 *     unfolded `li v0,1` in updateAudioScreen and the s7/s8 webs in audio_updateButtons).
 *  4. Old-style declarations with the right return type: MBOX_FindTexture_WORK(), MBOX_HideObject(), MBOX_ShowObject().
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    s32 type;            /* 0x00 */
    s32 handle;          /* 0x04 */
    f32 ang;             /* 0x08 */
    f32 mat[3][3];       /* 0x0C */
    f32 pos[3];          /* 0x30 */
    u8 pad3C[4];
} Button; /* 0x40 */

extern s32 audio_buttonsInited;
extern s32 audio_buttonsJustInited;
extern f32 audio_gButtonScale;
extern Button audio_buttonInfo[];
extern s32 gAudioMenuSelect;
extern f32 unitmatrix[];
extern volatile f32 gFrameSeconds;
extern volatile u8 gNumModels;
extern f32 audio_cursorSpeed;

f32 fabsf(f32);
#pragma intrinsic(fabsf)
void *memcpy(void *, const void *, u32);
intptr_t MBOX_FindTexture_WORK();
void ZOID_SetTexture(s16 handle, intptr_t tex, s32 c);
void WPitchUV(f32 angle, f32 uv[][3]);
void WRollUV(f32 angle, f32 uv[][3]);
void ScaleUV(f32 (*dst)[3], f32 (*src)[3], f32 s);
void MBOX_HideObject();
void MBOX_ShowObject();

s32 audio_validOption(s32 i) {
    s32 v;
    v = 1;
    return v;
}

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
typedef struct Msg { s16 id; s8 type, used; u8 payload[20]; } Msg;
typedef struct OSMesgQueue OSMesgQueue;
typedef void *OSMesg;
extern u32 gGameState;
extern s32 gNewGameState;
extern Stick gContStick[4];
extern s32 gActiveEdges;
extern s32 gContEdges[4];
extern s32 playerInPause;
extern s8 audio_inited;
extern s32 exitAudioScreen;
extern s32 gAudioMenuSelect;
extern s32 audio_gSong;
extern s32 gLastSong;
extern s8 audio_gControllerRemoved;
extern AudioCfg gOptions;
extern Pad16 gControllerData[];
extern s8 gSoftReset;
extern s32 audio_lock_queue, audio_command_queue;
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
Msg *GetCmd(void);
void audio_lock(void);
void audio_unlock(void);
void audio_initScreen(void);
void PlayStartABSound(s32 a);
void PlayLRSound(s32 a);
void PlayUDSound(s32 a);
void AudSetMasterVol(f32 blend, f32 amount, s32 first, s32 second);
void AudPlaySong(s32 a, s32 b);
void StartGameMusicInternal(s32 mode);
void StartGameMusic(void);
void AudSetMusicVol(f32 blend, f32 amount);
void AudSetFxVol(f32 blend, f32 amount);
void AudMono(s32 a);
void audio_TitlePane(void);
void audio_uninitScreen(void);
void setPauseState(s32 a);
void audio_updateButtons(void);

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The stop of the song (command 1) posted here is then
 * dropped: with no message the lock is given back and nothing is sent, and a message the full queue refuses is
 * given back (its in-use byte, +3). Nothing else is made here; the song plays on to the next stop. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){audio_unlock();return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)(m)->used=0



void AudKillSong(void);
void updateAudioScreen(void) {
    Pad16 *p;
    s32 padB;
    s32 btn;
    f32 stick[2];
    s32 ds;
    s32 dz;

    if (gGameState & 0x7C03FFFE) {
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
        case 0:
            break;
        case 1:
            break;
        case 2:
            AudKillSong();
            AudSetMusicVol(gOptions.music / 10.0f, 0.05f);
            if (audio_gSong < 12)
                AudPlaySong(audio_gSong, 1);
            else if (audio_gSong == 12) {
                if (gGameState & 0x7C03FFFE)
                    AudPlaySong(6, 1);
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
        if (gAudioMenuSelect == 1 || gAudioMenuSelect == 2) {
            AudSetMusicVol(gOptions.music / 10.0f, 0.05f);
            if (audio_gSong < 12)
                AudPlaySong(audio_gSong, 1);
            else if (audio_gSong == 12) {
                if (gGameState & 0x7C03FFFE)
                    AudPlaySong(6, 1);
                else
                    StartGameMusic();
            }
        }
    } else if (btn & 0x3000) {
        dz = (btn & 0x1000) ? -1 : 1;
        PlayLRSound(btn);
        switch (gAudioMenuSelect) {
        case 0:
            gOptions.sfx += dz;
            if (gOptions.sfx > 10)
                gOptions.sfx = 0;
            else if (gOptions.sfx < 0)
                gOptions.sfx = 10;
            AudSetFxVol(gOptions.sfx / 10.0f, 0.05f);
            break;
        case 1:
            gOptions.music += dz;
            if (gOptions.music > 10)
                gOptions.music = 0;
            else if (gOptions.music < 0)
                gOptions.music = 10;
            AudSetMusicVol(gOptions.music / 10.0f, 0.05f);
            break;
        case 2:
            AudKillSong();
            do {} while (0);
            audio_gSong += dz;
            if (audio_gSong > 13)
                audio_gSong = 0;
            else if (audio_gSong < 0)
                audio_gSong = 13;
            gLastSong = audio_gSong;
            gOptions.track = audio_gSong;
            AudSetMusicVol(gOptions.music / 10.0f, 0.05f);
            if (audio_gSong < 12)
                AudPlaySong(audio_gSong, 1);
            else if (audio_gSong == 12) {
                if (gGameState & 0x7C03FFFE)
                    AudPlaySong(6, 1);
                else
                    StartGameMusic();
            }
            break;
        case 3:
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
        if (gGameState & 0x7C03FFFE)
            gNewGameState = 4;
        else
            setPauseState(1);
    } else if (exitAudioScreen) {
        audio_uninitScreen();
        if (gGameState & 0x7C03FFFE)
            gNewGameState = 16;
        else
            setPauseState(1);
    }
}
