/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Options-screen button pair (GC optionscr.c): updateOptionScreen = updateOptionScreen (kept) and its internal callee
 * options_updateButtons = updateButtons (unsaved s0-s8/f20-f30). DB1E0 body unchanged; DA2C0
 * pad-free rewrite (GC statics cursorSpeed$472/scrollSpeed$473 = options_cursorSpeed/options_scrollSpeed as function-local statics,
 * GC order of sel_y, 255 spellings, alpha reuse) plus closing levers:
 *  - GC ZOID_SetColor(s16 objnum, union *col)@zoidtexture.c as an inlined helper with one nested level
 *    (ZOID_SetColor_da -> ZOID_SetColorP_da, both non-static, not on keep): the 32 B of frame below `color`
 *    were these two sites' inline areas (2 sites x 2 levels x 8 B); 202 -> 59 rows, frame 120 -> 152 exact.
 *  - DISCLOSED DEVICES (compiled-out, no code):
 *    * two `if (yi) {}` after the button loop (weight-1 uses): yi's priority 22/14 = 1.57 sits between the
 *      &options_buttonsJustInited address web (1.48-1.55) and the two pointer webs (&B[i].pos, &validOption[i], 1.94), so yi
 *      takes s8 and the pointers s6/s7 as retail (one holder inside the loop over-raised yi: s6);
 *    * `do {} while (0);` after `color.c[3] = alpha`: separates the byte store from the helper's word load, so
 *      &color is not CSE'd into a register (retail: `sb v1,99(sp); lw v0,96(sp)` sp-relative);
 *    * `do {} while (0);` after the pre-loop WPitchUV(1.5707964f, ...) call: one more live block for the
 *      hoisted 1.5707964f constant only, breaking its exact 30/16 tie with 40.0f so 40.0f gets $f26 and
 *      1.5707964f $f28 as retail (force oracle: the tie was the whole residual).
 *  - `u0` is one unused named local (GC has a `tex` pointer local the N64 code does not use): frame 152 with or
 *    without it (8-B rounding), but without it texIdx/color sit 4 B higher (106/100 vs retail 102/96).
 * superseding: options_uninitScreen = GC uninitScreen@optionscr.c (184 B, internal; sole caller updateOptionScreen)
 * with GC uninitButtons@optionscr.c written in place (the 32-entry buttonInfo loop: ZOID_RemoveObject =
 * RemoveObjectTree, handle := -1; buttonsInited = options_buttonsInited := 0; inited = options_inited := 0). No shaping
 * devices: the natural indexed loop replaces one-line `slot = &options_buttonInfo; do {` .loc device.
 * GC-derived names: see list. GC updateButtons locals: buttonIdx, i, prevPos[3] (= old), selectedPos
 * (= sel_y), clearScrollSpeed (s8, = flag), whichItem, buttonY (= yi, int), tex, texIdx (u16 at sp+102),
 * buttonColor (union, sp+96).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef union {
    u8 c[4];
    u32 rgba;
} Color;

typedef struct {
    s32 type;            /* 0x00 */
    s32 handle;          /* 0x04 */
    f32 ang;             /* 0x08 */
    f32 mat[3][3];       /* 0x0C */
    f32 pos[3];          /* 0x30 */
    u8 alpha;            /* 0x3C */
    u8 pad3D[3];
} Button; /* 0x40 */

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct {
    u32 flags, w4;
    void *obj;
    f32 scale, w;
    u16 model;
    s16 child, next, mode;
    void *light, *tex[4], *rect, *clip, *pal;
    Color color;         /* 0x60 */
    u8 pad40[4];
} Model68;

extern s32 options_buttonsInited;
extern s32 options_buttonsJustInited;
extern s32 options_validOption[];
extern s16 options_gSelectedOption;
extern Color options_buttonColor;
extern Button options_buttonInfo[];
extern s16 gMenuCount;
extern f32 unitmatrix[];
extern volatile f32 gFrameSeconds;
extern volatile u8 gNumModels;
extern Model68 ObjInstanceList[];

f32 fabsf(f32);
#pragma intrinsic(fabsf)
void *memcpy(void *, const void *, u32);
s32 MBOX_FindTexture_WORK();
void ZOID_SetTexture(s16 handle, s32 tex, s32 c);
void WPitchUV(f32 angle, f32 uv[][3]);
void WRollUV(f32 angle, f32 uv[][3]);
void ScaleUV(f32 (*dst)[3], f32 (*src)[3], f32 s);
void MBOX_HideObject();
void MBOX_ShowObject();

extern u32 gGameState;
extern intptr_t options_gTitlePane;
extern intptr_t options_blit;
extern s8 options_inited;
void RemovePanes(s32 arg0);
void RemoveBlit(s32 sound_id);
void removeBG(void);
void RemoveObjectTree(s16 arg0, s32 arg1, s32 arg2);


void options_uninitScreen(void);

void ZOID_SetColorP_da(s16 objnum, Color *col)
{
    ObjInstanceList[objnum].color.rgba = col->rgba;
}

void ZOID_SetColor_da(s16 objnum, Color *col)
{
    ZOID_SetColorP_da(objnum, col);
}


typedef struct {
    volatile s8 b0;
    u8 pad[15];
} Pad16;

extern s32 gNewGameState;
extern s32 playerInPause;
extern s32 gActiveEdges;
extern s32 gActiveAutoEdges;
extern s32 gContEdges[];
extern s32 gAutoEdges[];
extern s8 options_inited;
extern s32 exitOptionScreen;
extern s16 gOptionMenuSize;
extern s16 options_gFirstOption;
extern s8 gOptions[];
extern s16 gLang;
extern Pad16 gControllerData[];
extern s8 options_gControllerRemoved;
extern s8 gSoftReset;

void options_initScreen(void);
void PlayStartABSound(s32);
void PlayUDSound(s32);
void PlayLRSound(s32);
void LoadTextWave(void);
void SetOptions(void);
void options_TitlePane(void);
void options_uninitScreen(void);
void setPauseState(s32);

