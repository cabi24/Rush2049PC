/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * replay_save_prompt (0x800DABDC, 1540 B; historical label) = GC initScreen@optionscr.c (MAT3 optionscr.c, GC
 * 0x80074DE8) with GC calcOptionMenuMaxSize (0x80075BB0) and initButtons (0x80075D00) as real helpers that umerge
 * inlines (as in retail; GC file order: both defined after initScreen). Internal: sole caller updateOptionScreen =
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
 * Data: validOption options_validOption[14], gMenuCount gMenuCount, gOptionMenuSize gOptionMenuSize, gSelectedOption
 * options_gSelectedOption, gFirstOption options_gFirstOption, exitOptionScreen exitOptionScreen, blit options_blit, frontEndBlit options_frontEndBlit /
 * pauseBlit options_pauseBlit, gControllerRemoved options_gControllerRemoved, inited options_inited, buttonInfo options_buttonInfo[32], buttonNames
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

extern OSMesgQueue text_lock_queue;
extern Button options_buttonInfo[32];              /* buttonInfo */
extern char *D_801164E8[];                 /* buttonNames */
extern Ent ObjInstanceList[];
extern volatile u8 gNumModels;
extern f32 unitmatrix[3][3];               /* unitmatrix */
extern const MultiBlit options_frontEndBlit, options_pauseBlit;   /* frontEndBlit, pauseBlit */
extern Blit *options_blit;                   /* blit */
extern s32 options_buttonsInited, options_buttonsJustInited;         /* buttonsInited, buttonsJustInited */
extern s32 options_validOption[14];                 /* validOption */
extern s16 gMenuCount;                     /* gMenuCount */
extern s16 gOptionMenuSize;                     /* gOptionMenuSize */
extern s16 options_gSelectedOption;                     /* gSelectedOption */
extern s16 options_gFirstOption;                     /* gFirstOption */
extern s32 exitOptionScreen;                     /* exitOptionScreen */
extern s8 options_gControllerRemoved;                      /* gControllerRemoved */
extern s32 playerInPause;                     /* playerInPause */
extern ContData gControllerData[];              /* gControllerData */
extern s8 options_inited;                      /* inited */
extern s8 gGlobalsSet;
extern u32 gGameState;                   /* gGameState */

s32 osRecvMesg(OSMesgQueue *, void *, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32);
s16 TB_GetFontLineHeight(void);         /* TB_GetFontLineHeight */
void *memcpy(void *, const void *, u32);
s32 MBOX_FindObject_WORK(char *, s8, s8, s32);       /* MBOX_FindObject_WORK */
s16 MBOX_NewObject(s32, void *, s32, u32);       /* MBOX_NewObject wrapper */
TexDef *MBOX_FindTexture_WORK(char *, u16 *, s8, s8, s32); /* MBOX_FindTexture_WORK */
void ZOID_SetTexture(s16, TexDef *, s32);            /* ZOID_SetTexture */
void WPitchUV(f32, f32 [3][3]);               /* WPitchUV */
void ScaleUV(f32 [3][3], f32 [3][3], f32);   /* ScaleUV */
void SaveGlobals(void);                          /* SaveGlobals */
void createBG(void);                  /* createBG */
void ZeroCameras(void);                  /* ZeroCameras */
void ClearRushControls(void);                    /* ClearRushControls */
void options_TitlePane(void);                    /* _TitlePane@optionscr.c */
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);
Blit *NewMultiBlit(s16, s16, const MultiBlit *, s16);   /* NewMultiBlit */

s32 op_HAL_SetFont(s32 sel);



void op_calcOptionMenuMaxSize(void);
void op_initButtons(void);

void options_initScreen(void)
{
    s16 y;
    s32 u0;

    SaveGlobals();
    op_calcOptionMenuMaxSize();
    options_gSelectedOption = 0;
    while (!options_validOption[options_gSelectedOption]) {
        if (++options_gSelectedOption >= 14) options_gSelectedOption = 0;
    }
    options_gFirstOption = options_gSelectedOption;
    exitOptionScreen = 0;
    if (gGameState & 0x7C03FFFE) {
        createBG();
        op_initButtons();
        options_blit = NewMultiBlit(0, 0, &options_frontEndBlit, 1);
    } else {
        options_gControllerRemoved = !gControllerData[playerInPause].attached;
        op_HAL_SetFont(13);
        options_TitlePane();
        y = TB_GetFontLineHeight() * 2 + 10;
        op_HAL_SetFont(11);
        InitPane(40, y - 4, 280, y + (gOptionMenuSize * TB_GetFontLineHeight() + 4), 176, 0, 0, 0);
        options_blit = NewMultiBlit(0, 0, &options_pauseBlit, 1);
    }
    ClearRushControls();
    options_inited = 1;
}

void op_calcOptionMenuMaxSize(void);

void op_initButtons(void);
