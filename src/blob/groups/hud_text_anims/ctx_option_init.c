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
 *  - the button colours go through ZOID_SetColor, as MAT3 initButtons does (the N64 inlined it).
 *  - Dreamcast optionscr.c confirms the N64 height base 210 (GC 214).
 * DISCLOSED DEVICE: one unused local `u0` in replay_save_prompt = the exact frame residual. Retail frame 256, without
 * it 248 (every sp-relative offset already retail's); one 4-byte slot, frame rounded to 8 -> 256. GC DWARF lists only
 * `y` for initScreen; the source of the extra 8 B above the inline areas is unknown.
 * Data: validOption options_validOption[14], gMenuCount gMenuCount, gOptionMenuSize gOptionMenuSize, gSelectedOption
 * options_gSelectedOption, gFirstOption options_gFirstOption, exitOptionScreen exitOptionScreen, blit options_blit, frontEndBlit options_frontEndBlit /
 * pauseBlit options_pauseBlit, gControllerRemoved options_gControllerRemoved, inited options_inited, buttonInfo options_buttonInfo[32], buttonNames
 * options_buttonNames, buttonColor template D_80116DAC.
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
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 color; u8 pad40[4]; } Ent;
typedef union Color { u8 c[4]; u32 rgba; } Color;
typedef struct { s8 attached; u8 pad[15]; } ContData;

extern OSMesgQueue text_lock_queue;
extern Button options_buttonInfo[32];              /* buttonInfo */
extern char *options_buttonNames[];                 /* buttonNames */
extern Ent ObjInstanceList[];
extern volatile u8 gNumModels;
extern f32 unitmatrix[3][3];               /* unitmatrix */
extern const MultiBlit options_frontEndBlit, options_pauseBlit;   /* frontEndBlit, pauseBlit */
extern Blit *options_blit;                   /* blit */
extern s32 options_buttonsInited, options_buttonsJustInited;         /* buttonsInited, buttonsJustInited */
extern s32 options_validOption[14];                 /* validOption */
extern s16 gMenuCount;
extern s16 gOptionMenuSize;
extern s16 options_gSelectedOption;                     /* gSelectedOption */
extern s16 options_gFirstOption;                     /* gFirstOption */
extern s32 exitOptionScreen;
extern s8 options_gControllerRemoved;                      /* gControllerRemoved */
extern s32 playerInPause;
extern ContData gControllerData[];              /* gControllerData */
extern s8 options_inited;                      /* inited */
extern s8 gGlobalsSet;
extern u32 gGameState;

s32 osRecvMesg(OSMesgQueue *, void *, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32);
s16 TB_GetFontLineHeight(void);
void *memcpy(void *, const void *, u32);
s32 MBOX_FindObject_WORK(char *, s8, s8, s32);
s16 MBOX_NewObject(s32, void *, s32, u32);       /* MBOX_NewObject wrapper */
TexDef *MBOX_FindTexture_WORK(char *, u16 *, s8, s8, s32);
void ZOID_SetTexture(s16, TexDef *, s32);
void WPitchUV(f32, f32 [3][3]);
void ScaleUV(f32 [3][3], f32 [3][3], f32);
void SaveGlobals(void);
void createBG(void);
void ZeroCameras(void);
void ClearRushControls(void);
void options_TitlePane(void);                    /* _TitlePane@optionscr.c */
void *InitPane(s32, s32, s32, s32, s32, s32, s32, s32);
Blit *NewMultiBlit(s16, s16, const MultiBlit *, s16);





void op_calcOptionMenuMaxSize(void);
void op_initButtons(void);

s32 HAL_SetFont(s32 fontID);
void op_calcOptionMenuMaxSize(void)
{
    s32 height;
    s32 i;

    HAL_SetFont(13);
    height = 210 - TB_GetFontLineHeight() * 2;
    HAL_SetFont(11);
    height -= TB_GetFontLineHeight();
    gMenuCount = 0;
    for (i = 0; i < 14; i++) {
        if (options_validOption[i]) gMenuCount++;
    }
    gOptionMenuSize = gMenuCount < height / TB_GetFontLineHeight() ? gMenuCount : height / TB_GetFontLineHeight();
}

void ZOID_SetColor(s16 objnum, Color *col);
void op_initButtons(void)
{
    Color buttonColor = { { 255, 255, 255, 255 } };
    TexDef *tex;
    u16 texIdx;
    s32 buttonIdx;

    for (buttonIdx = 0; buttonIdx < 32; buttonIdx++) {
        if (options_buttonInfo[buttonIdx].handle == -1) {
            memcpy(options_buttonInfo[buttonIdx].mat, unitmatrix, 36);
            options_buttonInfo[buttonIdx].handle = MBOX_NewObject(
                MBOX_FindObject_WORK(options_buttonNames[options_buttonInfo[buttonIdx].type], 0, gNumModels - 1, 1),
                &options_buttonInfo[buttonIdx].mat, -1,
                (options_buttonInfo[buttonIdx].type == 0 || options_buttonInfo[buttonIdx].type == 1 ||
                 options_buttonInfo[buttonIdx].type == 2) ? 0x42000 : 0);
            ZOID_SetColor(options_buttonInfo[buttonIdx].handle, &buttonColor);
        }
    }
    tex = MBOX_FindTexture_WORK("CURSOR", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(options_buttonInfo[28].handle, tex, -1);
    tex = MBOX_FindTexture_WORK("B1", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(options_buttonInfo[29].handle, tex, -1);
    memcpy(options_buttonInfo[29].mat, unitmatrix, 36);
    WPitchUV(-1.5707964f, options_buttonInfo[29].mat);
    options_buttonInfo[29].pos[0] = -75.1f;
    options_buttonInfo[29].pos[1] = 55.2f;
    options_buttonInfo[29].pos[2] = 100.0f;
    ScaleUV(options_buttonInfo[29].mat, options_buttonInfo[29].mat, 0.5f);
    memcpy(options_buttonInfo[30].mat, unitmatrix, 36);
    WPitchUV(-1.5707964f, options_buttonInfo[30].mat);
    options_buttonInfo[30].pos[0] = -87.1f;
    options_buttonInfo[30].pos[1] = 55.2f;
    options_buttonInfo[30].pos[2] = 100.0f;
    tex = MBOX_FindTexture_WORK("BUTTON_TITLE", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(options_buttonInfo[31].handle, tex, -1);
    memcpy(options_buttonInfo[31].mat, unitmatrix, 36);
    WPitchUV(-1.5707964f, options_buttonInfo[31].mat);
    options_buttonInfo[31].pos[0] = 0.0f;
    options_buttonInfo[31].pos[1] = 115.0f;
    options_buttonInfo[31].pos[2] = 200.0f;
    options_buttonsInited = 1;
    options_buttonsJustInited = 1;
    ZeroCameras();
}
