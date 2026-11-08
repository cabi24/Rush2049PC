/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * replay_save_prompt (0x800DABDC, 1540 B; historical label) = GC initScreen@optionscr.c (MAT3 optionscr.c, GC
 * 0x80074DE8) with GC calcOptionMenuMaxSize (0x80075BB0) and initButtons (0x80075D00) as real helpers that umerge
 * inlines (as in retail; GC file order: both defined after initScreen). Internal: sole caller func_800DB1E0 =
 * updateOptionScreen (this group). Base: option_init/best.c (7 words); closing levers:
 *  - calcOptionMenuMaxSize / initButtons as separate (non-static, not on keep) helpers: retail's frame has their
 *    umerge inline areas ABOVE initButtons' locals (buttonColor sp+156, texIdx sp+150).
 *  - HAL_SetFont written as MAT3 HAL_text.c HAL_SetFont: one function with _GetSemaphore/_ReleaseSemaphore as
 * nested helpers (op_gfx_lock/op_gfx_unlock). The three-level form (op_SetFont -> op_font_set -> lock)
 *    is inlined at zero growth BEFORE the single-caller helpers are considered and pushes them over umerge's
 *    per-edge size limit; this form goes through umerge's later knapsack pass, after both helpers are inlined.
 *  - ZOID_SetColor with one nested level (op_setButtonColor -> op_ZOID_SetColorP), as the same file's
 *    updateButtons (ZOID_SetColor_da -> ZOID_SetColorP_da, buttons.c): +8 B of inline area below buttonColor.
 *  - Dreamcast optionscr.c confirms the N64 height base 210 (GC 214).
 * DISCLOSED DEVICE: one unused local `u0` in replay_save_prompt = the exact frame residual. Retail frame 256, without
 * it 248 (every sp-relative offset already retail's); one 4-byte slot, frame rounded to 8 -> 256. GC DWARF lists only
 * `y` for initScreen; the source of the extra 8 B above the inline areas is unknown.
 * Data: validOption D_80116D14[14], gMenuCount D_80149D9C, gOptionMenuSize D_80149B84, gSelectedOption
 * D_80116D9C, gFirstOption D_80149DA2, exitOptionScreen D_80116DA0, blit D_80116D98, frontEndBlit D_80116D4C /
 * pauseBlit D_80116D70, gControllerRemoved D_80116DA8, inited D_80116D94, buttonInfo D_8011650C[32], buttonNames
 * D_801164E8, buttonColor template D_80116DAC.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct OSMesgQueue OSMesgQueue;
typedef struct TexDef TexDef;
typedef struct Blit Blit;
typedef struct MultiBlit MultiBlit;
typedef struct { s32 type, handle; f32 ang; f32 mat[3][3]; f32 pos[3]; u8 pad3C[4]; } Button;   /* 0x40 */
typedef struct { u8 pad0[60]; u32 color; u8 pad40[4]; } Ent;                                 /* 0x44 */
typedef union Color { u8 c[4]; u32 rgba; } Color;
typedef struct { s8 attached; u8 pad[15]; } ContData;

extern OSMesgQueue D_801461D0;
extern Button D_8011650C[32];              /* buttonInfo */
extern char *D_801164E8[];                 /* buttonNames */
extern Ent D_8012E700[];
extern volatile u8 gNumModels;
extern f32 D_8011418C[3][3];               /* unitmatrix */
extern const MultiBlit D_80116D4C, D_80116D70;   /* frontEndBlit, pauseBlit */
extern Blit *D_80116D98;                   /* blit */
extern s32 D_80116D0C, D_80116D10;         /* buttonsInited, buttonsJustInited */
extern s32 D_80116D14[14];                 /* validOption */
extern s16 D_80149D9C;                     /* gMenuCount */
extern s16 D_80149B84;                     /* gOptionMenuSize */
extern s16 D_80116D9C;                     /* gSelectedOption */
extern s16 D_80149DA2;                     /* gFirstOption */
extern s32 D_80116DA0;                     /* exitOptionScreen */
extern s8 D_80116DA8;                      /* gControllerRemoved */
extern s32 D_8015698C;                     /* playerInPause */
extern ContData gControllerData[];              /* gControllerData */
extern s8 D_80116D94;                      /* inited */
extern s8 D_80117428;
extern u32 state_word_a;                   /* gGameState */

s32 osRecvMesg(OSMesgQueue *, void *, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 slot_state_setup(s32);
s16 object_bytes_sum_global(void);         /* TB_GetFontLineHeight */
void *memcpy(void *, const void *, u32);
s32 string_copy_format(char *, s8, s8, s32);       /* MBOX_FindObject_WORK */
s16 sign_extend_call(s32, void *, s32, u32);       /* MBOX_NewObject wrapper */
TexDef *func_800B24EC(char *, u16 *, s8, s8, s32); /* MBOX_FindTexture_WORK */
void func_8008D870(s16, TexDef *, s32);            /* ZOID_SetTexture */
void func_800B5898(f32, f32 [3][3]);               /* WPitchUV */
void func_8008B32C(f32 [3][3], f32 [3][3], f32);   /* ScaleUV */
void func_800B8810(void);                          /* SaveGlobals */
void createBG(void);                  /* createBG */
void particle_velocity_set(void);                  /* ZeroCameras */
void entity_audio_update(void);                    /* ClearRushControls */
void options_TitlePane(void);                    /* _TitlePane@optionscr.c */
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *ambient_sound_set(s32, s32, s32, s32, s32, intptr_t, s32, s32);
Blit *sound_control(s16, s16, const MultiBlit *, s16);   /* NewMultiBlit */

s32 op_HAL_SetFont(s32 sel);



void op_calcOptionMenuMaxSize(void);
void op_initButtons(void);

void options_initScreen(void)
{
    s16 y;
    s32 u0;

    func_800B8810();
    op_calcOptionMenuMaxSize();
    D_80116D9C = 0;
    while (!D_80116D14[D_80116D9C]) {
        if (++D_80116D9C >= 14) D_80116D9C = 0;
    }
    D_80149DA2 = D_80116D9C;
    D_80116DA0 = 0;
    if (state_word_a & 0x7C03FFFE) {
        createBG();
        op_initButtons();
        D_80116D98 = sound_control(0, 0, &D_80116D4C, 1);
    } else {
        D_80116DA8 = !gControllerData[D_8015698C].attached;
        op_HAL_SetFont(13);
        options_TitlePane();
        y = object_bytes_sum_global() * 2 + 10;
        op_HAL_SetFont(11);
        ambient_sound_set(40, y - 4, 280, y + (D_80149B84 * object_bytes_sum_global() + 4), 176, 0, 0, 0);
        D_80116D98 = sound_control(0, 0, &D_80116D70, 1);
    }
    entity_audio_update();
    D_80116D94 = 1;
}

void op_calcOptionMenuMaxSize(void);

void op_initButtons(void);
