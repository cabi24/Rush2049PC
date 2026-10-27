/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_8010D85C (historical label): capture-the-flag flag object setup callback.
 * mode == 0: entity_transform_apply(obj, 1) (unlink) and return. Otherwise, unless D_801170FC is set,
 * the object's model is initialised once (model->flags bit 0): f5A = 10; the flag name table
 * D_80118E08 ("REDFLAG", "GRNFLAG", "BLUFLAG") is indexed by whether the model name contains "BLU" (2)
 * or "GRN" (1), looked up through sound_bank_load (historical label; 5 actual arguments, called
 * through an implicit declaration as retail passes a fifth word 1 at sp+16) and the result stored in
 * the 0x44-byte entity record D_8012E700[model->id] at +0x38; obj->team = digit after "_OFF" (0 if
 * absent). Finally obj->fn = audio_channel_setup (historical label; 0x80094888) and obj->f10 = 0.
 * No arcade ancestor (N64 battle/CTF mode).
 * Shaping quirks (each verified necessary): unused `char buf[16]` and unused `s32 b0` give the 88-byte
 * frame and the handle/name slots at sp+68/sp+64; `handle` is a word (u16 gives sp+70); D_80140BDC is
 * read through `volatile` (retail `lui; addiu; lbu 0(t4)`); one variable `i` carries the table index,
 * the entity id and the team digit (all in v1). The three name strings are this function's own data
 * (0x80121DA0..0x80121DAC, verified by the scorer).
 */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef unsigned short u16;
typedef int s32; typedef float f32;
#ifndef TARGET_PC
typedef unsigned int uintptr_t;
typedef int intptr_t;
#endif
#ifdef TARGET_PC
/* PC: obj is a visual (tVisual, 0x20 bytes: next and func are host pointers), and its data the target, a
 * level-object node, which keeps its N64 layout: both as 32-bit window addresses, as is the node's state, here the
 * flag's place record (its name; a track WAD record) (docs/port/CARS.md, POINTERS.md 10). */
enum n64p_Model { n64p_Model_ };
enum n64p_char { n64p_char_ };
typedef struct Model {
    u8 pad0[4];
    u8 flags;
    u8 pad5[9];
    s16 id;
    u8 pad10[0x5A - 0x10];
    s16 f5A;
    u8 pad5C[0x6C - 0x5C];
    enum n64p_char name;
} Model;
typedef struct Obj {
    void *next;
    s16 team;
    u8 pad6[6];
    enum n64p_Model model;
    f32 f10;
    void (*fn)();
} Obj;
#define N64P(x) _Generic((x), enum n64p_Model: (Model *)(uintptr_t)(x), enum n64p_char: (char *)(uintptr_t)(x), \
    default: (x))
#else
typedef struct Model {
    u8 pad0[4];
    u8 flags;
    u8 pad5[9];
    s16 id;
    u8 pad10[0x5A - 0x10];
    s16 f5A;
    u8 pad5C[0x6C - 0x5C];
    char *name;
} Model;
typedef struct Obj {
    u8 pad0[4];
    s16 team;
    u8 pad6[6];
    Model *model;
    f32 f10;
    void (*fn)();
} Obj;
#define N64P(x) x
#endif
#ifdef TARGET_PC
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9).
 * `sound` is the palette slot (N64 +0x38): MBOX_FindPalette_WORK returns the WAD's palette record. */
typedef struct Ent { unsigned int flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip; intptr_t sound; unsigned int prim, env; } Ent;
#else
typedef struct Ent { u8 pad0[0x38]; s32 sound; u8 pad3C[8]; } Ent;
#endif
extern s32 D_801170FC;
extern char *D_80118E08[];
extern volatile u8 gNumModels;
extern Ent D_8012E700[];
void entity_transform_apply(void *node, s32 unlink);
char *func_800A464C(char *s, char *find);
void audio_channel_setup();
#ifdef TARGET_PC
/* PC: retail calls it through an implicit declaration (an int result) with a fifth word; the definition
 * returns the palette record, which an int would cut to 32 bits. Old-style, so the five-word call stays. */
intptr_t MBOX_FindPalette_WORK();
#endif

void func_8010D85C(Obj *obj, s16 mode) {
    char buf[16];
    s32 handle;
    char *name;
    Model *m;
    char *p;
    s32 i;
    intptr_t r;
    s32 b0;

    if (mode == 0) {
        entity_transform_apply(obj, 1);
        return;
    }
    if (D_801170FC != 0) {
        return;
    }
    m = N64P(obj->model);
    if (!(m->flags & 1)) {
        name = N64P(m->name);
        m->f5A = 10;
        if (func_800A464C(name, "BLU") != 0) {
            i = 2;
        } else if (func_800A464C(name, "GRN") != 0) {
            i = 1;
        } else {
            i = 0;
        }
        r = MBOX_FindPalette_WORK(D_80118E08[i], &handle, 0, (s8)(gNumModels - 1), 1);
        i = m->id;
        D_8012E700[i].sound = r;
        p = func_800A464C(N64P(m->name), "_OFF");
        if (p != 0) {
            i = (s8)(p[4] - '0');
        } else {
            i = 0;
        }
        obj->team = i;
        m->flags |= 1;
    }
    obj->fn = audio_channel_setup;
    obj->f10 = 0.0f;
}
