/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * AnimateFlag (historical label): capture-the-flag flag object setup callback.
 * mode == 0: ReleaseEnviron(obj, 1) (unlink) and return. Otherwise, unless gPauseState is set,
 * the object's model is initialised once (model->flags bit 0): f5A = 10; the flag name table
 * flag_pals ("REDFLAG", "GRNFLAG", "BLUFLAG") is indexed by whether the model name contains "BLU" (2)
 * or "GRN" (1), looked up through sound_bank_load (historical label; 5 actual arguments, called
 * through an implicit declaration as retail passes a fifth word 1 at sp+16) and the result stored in
 * the 0x44-byte entity record ObjInstanceList[model->id] at +0x38; obj->team = digit after "_OFF" (0 if
 * absent). Finally obj->fn = AnimateObjSwapGeneric (historical label; 0x80094888) and obj->f10 = 0.
 * No arcade ancestor (N64 battle/CTF mode).
 * Shaping quirks (each verified necessary): unused `char buf[16]` and unused `s32 b0` give the 88-byte
 * frame and the handle/name slots at sp+68/sp+64; `handle` is a word (u16 gives sp+70); gNumModels is
 * read through `volatile` (retail `lui; addiu; lbu 0(t4)`); one variable `i` carries the table index,
 * the entity id and the team digit (all in v1). The three name strings are this function's own data
 * (0x80121DA0..0x80121DAC, verified by the scorer).
 */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef unsigned short u16;
typedef int s32; typedef float f32;
/* PC: obj is a visual (tVisual, 0x20 bytes: next and func are host pointers), and its data the target, a
 * level-object node, which keeps its N64 layout: both as 32-bit window addresses, as is the node's state, here the
 * flag's place record (its name; a track WAD record) (docs/port/CARS.md, POINTERS.md 10). */
enum n64p_Model { n64p_Model_ };
enum n64p_char { n64p_char_ };
typedef struct Model {
    u8 pad0[4];
    u8 flags;
    u8 pad5[7];
    s32 id;                 /* the target's handle, whose low half is the model id (N64 +0xE): +0xC here */
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
#define N64P(x) _Generic((x), enum n64p_Model: (Model *)PC_N64P(x), enum n64p_char: (char *)PC_N64P(x), \
    default: (x))
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9).
 * `sound` is the palette slot (N64 +0x38): MBOX_FindPalette_WORK returns the WAD's palette record. */
typedef struct Ent { unsigned int flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip; intptr_t sound; unsigned int prim, env; } Ent;
extern s32 gPauseState;
extern char *flag_pals[];
extern volatile u8 gNumModels;
extern Ent ObjInstanceList[];
void ReleaseEnviron(void *node, s32 unlink);
char *game_strstr(char *s, char *find);
void AnimateObjSwapGeneric();
/* PC: retail calls it through an implicit declaration (an int result) with a fifth word; the definition
 * returns the palette record, which an int would cut to 32 bits. Old-style, so the five-word call stays. */
intptr_t MBOX_FindPalette_WORK();

void AnimateFlag(Obj *obj, s16 mode) {
    char buf[16];
    s32 handle;
    char *name;
    Model *m;
    char *p;
    s32 i;
    intptr_t r;
    s32 b0;

    if (mode == 0) {
        ReleaseEnviron(obj, 1);
        return;
    }
    if (gPauseState != 0) {
        return;
    }
    m = N64P(obj->model);
    if (!(m->flags & 1)) {
        name = N64P(m->name);
        m->f5A = 10;
        if (game_strstr(name, "BLU") != 0) {
            i = 2;
        } else if (game_strstr(name, "GRN") != 0) {
            i = 1;
        } else {
            i = 0;
        }
        r = MBOX_FindPalette_WORK(flag_pals[i], &handle, 0, (s8)(gNumModels - 1), 1);
        i = m->id;
        ObjInstanceList[i].sound = r;
        p = game_strstr(N64P(m->name), "_OFF");
        if (p != 0) {
            i = (s8)(p[4] - '0');
        } else {
            i = 0;
        }
        obj->team = i;
        m->flags |= 1;
    }
    obj->fn = AnimateObjSwapGeneric;
    obj->f10 = 0.0f;
}
