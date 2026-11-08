/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * drone_pathfind_main (0x800D71D0, 1124 B; historical label) = GC initScreen@audioscr.c (MAT3 audioscr.c, GC 0x8006600C)
 * with GC initButtons@audioscr.c (GC 0x80066DD8) written in place. Internal: sole caller updateAudioScreen =
 * updateAudioScreen (this group); unsaved s0-s8 (the caller saves them).
 * Body: (mat3_ui_audio_init_20261017/group/init.c, credited), adapted for the
 * whole-program unit by:
 *  - initButtons is written in place: as a helper the unit's umerge knapsack (umerge f_pack_knapsack is global over the
 *    program) does not inline it once the other helpers here are inlined; retail inlined it.
 *  - AudKillSong (N64 AudKillSong, never called by jal in retail) is the local helper pair di_post_type1 /
 *    di_ProcessCommand (GC AudKillSong -> _ProcessCommand(cmd)); calling the shared kept AudKillSong is not
 *    inlined in the unit (same knapsack). `task = GetCmd(); task->type = 1;` on one line gives retail's
 *    `li t8,1; sb t8,2(v0); move s0,v0` order (as1 line tie).
 * - di_setButtonColor = GC ZOID_SetColor (non-static, not on keep: stub rule).
 * N64 differences from GC kept as in: 12 named songs + random/off, table indices, random song 6, button y 55.2.
 * DISCLOSED DEVICES:
 *  1. `do {} while (0);` after the last ZOID_SetTexture (ZOID_SetTexture(buttonInfo[11]...)): a block end that adds a
 *     live block to the unitmatrix address web (unitmatrix), breaking its exact priority tie (save 13/7 each) with the
 *     gNumModels address web, so &gNumModels takes s8 and unitmatrix is rematerialised per memcpy as in retail
 *     (force oracle p1:w123=s,p1:w125=c22 -> only frame rows left).
 *  2. Unused locals u0..u3 = the exact frame residual: retail frame 168, without them 152 (4 slots, 16 B, all at the
 *     frame top: every sp-relative access is identical with and without them). Probably retail's umerge inline area
 *     of initButtons, which the unit cannot reproduce (see above).
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16; typedef int s32; typedef unsigned int u32; typedef float f32;
typedef struct TexDef TexDef; typedef struct Blit Blit; typedef struct MultiBlit MultiBlit;
typedef struct { s32 type, handle; f32 ang; f32 mat[3][3]; f32 pos[3]; u8 pad3C[4]; } Button;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Ent {
    /* 0x00 */ s32 w0;
    /* 0x04 */ s32 w4;
    /* 0x08 */ void *owner;
    /* 0x10 */ f32 scaleA;
    /* 0x14 */ f32 scaleB;
    /* 0x18 */ u16 id;          /* 0xFFFF = free */
    /* 0x1A */ s16 link[3];     /* -1 = none */
    /* 0x20 */ void *w28[8];
    u32 color;
    /* 0x64 */ s32 w64;
} Ent; /* size 0x68 */


typedef union Color { u8 c[4]; u32 rgba; } Color;
typedef struct { u8 pad0[12]; s8 music; s8 sfx; s8 track; s8 mono; } AudioCfg;
extern AudioCfg gOptions;
extern Button audio_buttonInfo[12]; extern Ent ObjInstanceList[];
extern char *audio_buttonNames[]; extern u8 gNumModels; extern f32 unitmatrix[3][3];
extern char audio_init_texture_name_1[],audio_init_texture_name_2[],audio_init_texture_name_3[];
extern const struct pc_multiblit audio_frontEndBlit[1],audio_pauseBlit[2];   /* one blit and two, as NewMultiBlit gets them */
extern Blit *audio_blit;
extern s32 audio_buttonsInited,audio_buttonsJustInited,gAudioMenuSelect,exitAudioScreen,audio_gSong;
extern s8 gGlobalsSet,audio_inited; extern u32 gGameState;
void *memcpy(void *,const void *,u32);
s32 MBOX_FindObject_WORK(char *,s8,s8,s32);
s16 ZOID_AddObject(s32,void *,s16,s32);
TexDef *MBOX_FindTexture_WORK(char *,u16 *,s8,s8,s32);
void ZOID_SetTexture(s16,TexDef *,s32);
void WPitchUV(f32,f32 [3][3]); void ScaleUV(f32 [3][3],f32 [3][3],f32);
void createBG(void); void ZeroCameras(void); void ClearRushControls(void);
Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);
void audio_calcMenuRect(void);void AudKillSong(void);void AudSetMusicVol(f32,f32);void AudPlaySong(s32,s32);void StartGameMusic(void);
s16 MBOX_NewObject(s32,void *,s32,u32);void SaveGlobals(void);
void di_setButtonColor(s16 objnum,Color *col) { ObjInstanceList[objnum].color=col->rgba; }
typedef struct OSMesgQueue OSMesgQueue;
typedef struct Msg { s16 id; s8 type, used; u8 payload[20]; } Msg;
extern s32 audio_command_queue;
Msg *GetCmd(void);
void audio_lock(void);
void audio_unlock(void);
s32 osSendMesg(OSMesgQueue *, void *, s32);
/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The stop of the song (command 1) that di_post_type1 posts
 * is then dropped: with no message the lock is given back and nothing is sent, and a message the full queue
 * refuses is given back (its in-use byte, +3). Nothing else is made here; the song plays on to the next stop. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){audio_unlock();return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)(m)->used=0
void di_ProcessCommand(Msg *task)
{
    PC_CMD_SEND((OSMesgQueue *) &audio_command_queue, task);
}
void di_post_type1(void)
{
    Msg *task;
    audio_lock();
    task = GetCmd(); PC_CMD_OR_UNLOCK(task) task->type = 1;
    audio_unlock();
    di_ProcessCommand(task);
}
extern void SaveGlobals(void);
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
                di_setButtonColor(audio_buttonInfo[buttonIdx].handle, &buttonColor);
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
        di_post_type1();
        AudSetMusicVol(gOptions.music / 10.0f, 0.05f);
        if (audio_gSong < 12) AudPlaySong(audio_gSong,1);
        else if (audio_gSong == 12) {
            if (gGameState & 0x7C03FFFE) AudPlaySong(6,1);
            else StartGameMusic();
        }
    }
    audio_inited = 1;
}


