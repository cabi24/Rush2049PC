#include "game/game.h"

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

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
VIEW(Ent[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

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
