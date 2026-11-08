/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Options-screen button pair (GC optionscr.c): func_800DB1E0 = updateOptionScreen (kept) and its internal callee
 * func_800DA2C0 = updateButtons (unsaved s0-s8/f20-f30). DB1E0 body unchanged; DA2C0
 * pad-free rewrite (GC statics cursorSpeed$472/scrollSpeed$473 = D_8015273C/D_80152740 as function-local statics,
 * GC order of sel_y, 255 spellings, alpha reuse) plus closing levers:
 *  - GC ZOID_SetColor(s16 objnum, union *col)@zoidtexture.c as an inlined helper with one nested level
 *    (ZOID_SetColor_da -> ZOID_SetColorP_da, both non-static, not on keep): the 32 B of frame below `color`
 *    were these two sites' inline areas (2 sites x 2 levels x 8 B); 202 -> 59 rows, frame 120 -> 152 exact.
 *  - DISCLOSED DEVICES (compiled-out, no code):
 *    * two `if (yi) {}` after the button loop (weight-1 uses): yi's priority 22/14 = 1.57 sits between the
 *      &D_80116D10 address web (1.48-1.55) and the two pointer webs (&B[i].pos, &validOption[i], 1.94), so yi
 *      takes s8 and the pointers s6/s7 as retail (one holder inside the loop over-raised yi: s6);
 *    * `do {} while (0);` after `color.c[3] = alpha`: separates the byte store from the helper's word load, so
 *      &color is not CSE'd into a register (retail: `sb v1,99(sp); lw v0,96(sp)` sp-relative);
 *    * `do {} while (0);` after the pre-loop func_800B5898(1.5707964f, ...) call: one more live block for the
 *      hoisted 1.5707964f constant only, breaking its exact 30/16 tie with 40.0f so 40.0f gets $f26 and
 *      1.5707964f $f28 as retail (force oracle: the tie was the whole residual).
 *  - `u0` is one unused named local (GC has a `tex` pointer local the N64 code does not use): frame 152 with or
 *    without it (8-B rounding), but without it texIdx/color sit 4 B higher (106/100 vs retail 102/96).
 * superseding: func_800DA0BC = GC uninitScreen@optionscr.c (184 B, internal; sole caller func_800DB1E0)
 * with GC uninitButtons@optionscr.c written in place (the 32-entry buttonInfo loop: ZOID_RemoveObject =
 * entity_spawn_callback, handle := -1; buttonsInited = D_80116D0C := 0; inited = D_80116D94 := 0). No shaping
 * devices: the natural indexed loop replaces one-line `slot = &D_8011650C; do {` .loc device.
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

extern s32 D_80116D0C;
extern s32 D_80116D10;
extern s32 D_80116D14[];
extern s16 D_80116D9C;
extern Color D_80116DB0;
extern Button D_8011650C[];
extern s16 D_80149D9C;
extern f32 D_8011418C[];
extern volatile f32 D_8002EB94;
extern volatile u8 gNumModels;
extern Model68 D_8012E700[];

f32 fabsf(f32);
#pragma intrinsic(fabsf)
void *memcpy(void *, const void *, u32);
s32 func_800B24EC();
void func_8008D870(s16 handle, s32 tex, s32 c);
void func_800B5898(f32 angle, f32 uv[][3]);
void func_800B5940(f32 angle, f32 uv[][3]);
void func_8008B32C(f32 (*dst)[3], f32 (*src)[3], f32 s);
void model_data_load();
void model_transform_setup();

extern u32 state_word_a;
extern intptr_t D_80116DA4;
extern intptr_t D_80116D98;
extern s8 D_80116D94;
void sound_handles_clear(s32 arg0);
void sound_stop(s32 sound_id);
void ambient_sounds_clear(void);
void entity_spawn_callback(s16 arg0, s32 arg1, s32 arg2);


void func_800DA0BC(void);

void ZOID_SetColorP_da(s16 objnum, Color *col)
{
    D_8012E700[objnum].color.rgba = col->rgba;
}

void ZOID_SetColor_da(s16 objnum, Color *col)
{
    ZOID_SetColorP_da(objnum, col);
}


typedef struct {
    volatile s8 b0;
    u8 pad[15];
} Pad16;

extern u32 D_801174B4;
extern s32 D_801174B8;
extern s32 D_8015698C;
extern s32 D_8015694C;
extern s32 D_80149784;
extern s32 D_80156998[];
extern s32 D_80143A00[];
extern s8 D_80116D94;
extern s32 D_80116DA0;
extern s16 D_80149B84;
extern s16 D_80149DA2;
extern s8 D_80146108[];
extern s16 D_8013FEC8;
extern Pad16 gControllerData[];
extern s8 D_80116DA8;
extern s8 D_80157244;

void options_initScreen(void);
void resource_type_select(s32);
void audio_distance_atten(s32);
void audio_doppler(s32);
void func_800A510C(void);
void SetOptions(void);
void options_TitlePane(void);
void func_800DA0BC(void);
void setPauseState(s32);

