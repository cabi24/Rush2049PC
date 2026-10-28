/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * drone_pathfind_main (0x800D71D0, 1124 B; historical label) = GC initScreen@audioscr.c (MAT3 audioscr.c, GC 0x8006600C)
 * with GC initButtons@audioscr.c (GC 0x80066DD8) written in place. Internal: sole caller func_800D7634 =
 * updateAudioScreen (this group); unsaved s0-s8 (the caller saves them).
 * Body: (mat3_ui_audio_init_20261017/group/init.c, credited), adapted for the
 * whole-program unit by:
 *  - initButtons is written in place: as a helper the unit's umerge knapsack (umerge f_pack_knapsack is global over the
 *    program) does not inline it once the other helpers here are inlined; retail inlined it.
 *  - AudKillSong (N64 object_type1_create, never called by jal in retail) is the local helper pair di_post_type1 /
 *    di_ProcessCommand (GC AudKillSong -> _ProcessCommand(cmd)); calling the shared kept object_type1_create is not
 *    inlined in the unit (same knapsack). `task = func_80091B00(); task->type = 1;` on one line gives retail's
 *    `li t8,1; sb t8,2(v0); move s0,v0` order (as1 line tie).
 * - di_setButtonColor = GC ZOID_SetColor (non-static, not on keep: stub rule).
 * N64 differences from GC kept as in: 12 named songs + random/off, table indices, random song 6, button y 55.2.
 * DISCLOSED DEVICES:
 *  1. `do {} while (0);` after the last ZOID_SetTexture (func_8008D870(buttonInfo[11]...)): a block end that adds a
 *     live block to the unitmatrix address web (D_8011418C), breaking its exact priority tie (save 13/7 each) with the
 *     gNumModels address web, so &gNumModels takes s8 and unitmatrix is rematerialised per memcpy as in retail
 *     (force oracle p1:w123=s,p1:w125=c22 -> only frame rows left).
 *  2. Unused locals u0..u3 = the exact frame residual: retail frame 168, without them 152 (4 slots, 16 B, all at the
 *     frame top: every sp-relative access is identical with and without them). Probably retail's umerge inline area
 *     of initButtons, which the unit cannot reproduce (see above).
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16; typedef int s32; typedef unsigned int u32; typedef float f32;
typedef struct TexDef TexDef; typedef struct Blit Blit; typedef struct MultiBlit MultiBlit;
typedef struct { s32 type, handle; f32 ang; f32 mat[3][3]; f32 pos[3]; u8 pad3C[4]; } Button;
#ifdef TARGET_PC
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
#else
typedef struct Ent {
    /* 0x00 */ s32 w0;
    /* 0x04 */ s32 w4;
    /* 0x08 */ void *owner;
    /* 0x0C */ f32 scaleA;
    /* 0x10 */ f32 scaleB;
    /* 0x14 */ u16 id;          /* 0xFFFF = free */
    /* 0x16 */ s16 link[3];     /* -1 = none */
    /* 0x1C */ s32 w28[8];
    u32 color;
    /* 0x40 */ s32 w64;
} Ent; /* size 0x44 */
#endif


typedef union Color { u8 c[4]; u32 rgba; } Color;
typedef struct { u8 pad0[12]; s8 music; s8 sfx; s8 track; s8 mono; } AudioCfg;
extern AudioCfg D_80146108;
extern Button D_801102B4[12]; extern Ent D_8012E700[];
extern char *D_80110290[]; extern u8 gNumModels; extern f32 D_8011418C[3][3];
extern char D_8011F6F0[],D_8011F6F8[],D_8011F6FC[];
#ifdef TARGET_PC
extern const struct pc_multiblit D_801105C0[1],D_801105E4[2];   /* one blit and two, as NewMultiBlit gets them */
#else
extern const MultiBlit D_801105C0,D_801105E4;
#endif
extern Blit *D_80110630;
extern s32 D_801105B4,D_801105B8,D_80110638,D_8011063C,D_80110640;
extern s8 D_80117428,D_8011062C; extern u32 state_word_a;
void *memcpy(void *,const void *,u32);
s32 string_copy_format(char *,s8,s8,s32);
s16 func_8008E26C(s32,void *,s16,s32);
TexDef *func_800B24EC(char *,u16 *,s8,s8,s32);
void func_8008D870(s16,TexDef *,s32);
void func_800B5898(f32,f32 [3][3]); void func_8008B32C(f32 [3][3],f32 [3][3],f32);
void createBG(void); void particle_velocity_set(void); void entity_audio_update(void);
Blit *sound_control(s16,s16,const MultiBlit *,s16);
void audio_calcMenuRect(void);void object_type1_create(void);void speed_mode0_wrapper(f32,f32);void sync_entry_register(s32,s32);void physics_init_mode0(void);
s16 sign_extend_call(s32,void *,s32,u32);void func_800B8810(void);
void di_setButtonColor(s16 objnum,Color *col) { D_8012E700[objnum].color=col->rgba; }
typedef struct OSMesgQueue OSMesgQueue;
typedef struct Msg { s16 id; s8 type, used; u8 payload[20]; } Msg;
extern s32 D_801427A8;
Msg *func_80091B00(void);
void func_80091AF0(void);
void func_80091AF8(void);
s32 osSendMesg(OSMesgQueue *, void *, s32);
#ifdef TARGET_PC
/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's entity_transform_calc says when). The stop of the song (command 1) that di_post_type1 posts
 * is then dropped: with no message the lock is given back and nothing is sent, and a message the full queue
 * refuses is given back (its in-use byte, +3). Nothing else is made here; the song plays on to the next stop. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){func_80091AF8();return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)(m)->used=0
#else
#define PC_CMD_OR_UNLOCK(m)
#define PC_CMD_SEND(q,m) osSendMesg(q,m,0)
#endif
void di_ProcessCommand(Msg *task)
{
    PC_CMD_SEND((OSMesgQueue *) &D_801427A8, task);
}
void di_post_type1(void)
{
    Msg *task;
    func_80091AF0();
    task = func_80091B00(); PC_CMD_OR_UNLOCK(task) task->type = 1;
    func_80091AF8();
    di_ProcessCommand(task);
}
extern void func_800B8810(void);
void audio_initScreen(void)
{
    s32 u0, u1, u2, u3;

    func_800B8810();
    audio_calcMenuRect();
    if (state_word_a & 0x7C03FFFE) {
        createBG();
        {
        Color buttonColor = {255, 255, 255, 255};
        TexDef *tex;
        unsigned short texIdx;
        s32 buttonIdx;
        for (buttonIdx = 0; buttonIdx < 12; buttonIdx++) {
            if (D_801102B4[buttonIdx].handle == -1) {
                memcpy(D_801102B4[buttonIdx].mat, D_8011418C, 36);
                D_801102B4[buttonIdx].handle = sign_extend_call(
                    string_copy_format(D_80110290[D_801102B4[buttonIdx].type], 0,
                                       (s8)(gNumModels - 1), 1),
                    &D_801102B4[buttonIdx].mat, -1,
                    (D_801102B4[buttonIdx].type == 0 ||
                     D_801102B4[buttonIdx].type == 1 ||
                     D_801102B4[buttonIdx].type == 2) ? 0x42000 : 0);
                di_setButtonColor(D_801102B4[buttonIdx].handle, &buttonColor);
            }
        }
        tex = func_800B24EC(D_8011F6F0, &texIdx, 0, (s8)(gNumModels - 1), 1);
        func_8008D870(D_801102B4[8].handle, tex, -1);
        tex = func_800B24EC(D_8011F6F8, &texIdx, 0, (s8)(gNumModels - 1), 1);
        func_8008D870(D_801102B4[9].handle, tex, -1);
        memcpy(D_801102B4[9].mat, D_8011418C, 36);
        func_800B5898(-1.5707964f, D_801102B4[9].mat);
        D_801102B4[9].pos[0] = -75.1f;
        D_801102B4[9].pos[1] = 55.2f;
        D_801102B4[9].pos[2] = 100.0f;
        func_8008B32C(D_801102B4[9].mat, D_801102B4[9].mat, 0.5f);
        memcpy(D_801102B4[10].mat, D_8011418C, 36);
        func_800B5898(-1.5707964f, D_801102B4[10].mat);
        D_801102B4[10].pos[0] = -87.1f;
        D_801102B4[10].pos[1] = 55.2f;
        D_801102B4[10].pos[2] = 100.0f;
        tex = func_800B24EC(D_8011F6FC, &texIdx, 0, (s8)(gNumModels - 1), 1);
        func_8008D870(D_801102B4[11].handle, tex, -1);
        do {} while (0);
        memcpy(D_801102B4[11].mat, D_8011418C, 36);
        func_800B5898(-1.5707964f, D_801102B4[11].mat);
        D_801102B4[11].pos[0] = 0.0f;
        D_801102B4[11].pos[1] = 115.0f;
        D_801102B4[11].pos[2] = 200.0f;
        D_801105B4 = 1;
        D_801105B8 = 1;
        particle_velocity_set();
}
        D_80110630 = sound_control(0,0,&D_801105C0,1);
    } else D_80110630 = sound_control(0,0,&D_801105E4,2);
    entity_audio_update();
    D_8011063C = 0;
    D_80110640 = D_80146108.track;
    if (D_80110638 == 1 || D_80110638 == 2) {
        di_post_type1();
        speed_mode0_wrapper(D_80146108.music / 10.0f, 0.05f);
        if (D_80110640 < 12) sync_entry_register(D_80110640,1);
        else if (D_80110640 == 12) {
            if (state_word_a & 0x7C03FFFE) sync_entry_register(6,1);
            else physics_init_mode0();
        }
    }
    D_8011062C = 1;
}


