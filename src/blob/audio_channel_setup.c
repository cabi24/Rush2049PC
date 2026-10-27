/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * audio_channel_setup (0x80094888, 332 B) -- animation-frame stepper (setup callback installed by func_8010D85C).
 * mode 0 -> entity_transform_apply(obj, 1).  Otherwise (unless D_801170FC) timer -= D_8002EB94 (volatile f32
 * frame time); when it runs out: frame++, timer = 0.0625; at frame >= model->count look up the kind flags
 * D_80117530[model->type].flags: 0x1000 -> remove (shared apply tail), 0x2000 -> entity_spawn_callback(model->id,
 * 0, 0) then remove, else frame = 0.  Then f = (s16)(model->base + frame); if f != model->cur:
 * D_8012E700[model->id].tex = D_801427C0[f], model->cur = f. (Semantics)
 *
 * (strict MATCH, -O3 and -O2 standalone; EQUAL in the whole-program unit, 0 locked bodies differ).
 * Shaping device, disclosed: the compiled-out read `if (D_801427C0[f]) {}` placed BEFORE `id = m->id`.
 *  - Retail's `lui a2; addu a2,a2,t2; lhu a1,%lo(a2)` is as1 copy coalescing: ugen loads the table value into a
 *    coloured EXPRESSION web (a2) and copies it (u16 `and`) into the `tex` variable web (a1).  The extra read makes
 *    `D_801427C0[f]` a multi-occurrence expression web, so it is coloured instead of loaded straight into `tex`.
 *  - Its position before `id = m->id` puts id and tex in one block with equal priority (save 2.0); the tie goes
 *    to the lower web number (id), so id = v1, tex = a1, the expression = a2 as in retail (ctrace).  With the read
 *    after `id = m->id` (7 words) the force oracle p1:w48=c2,w54=c4,w53=c5 already gave 0 rows.
 * Arcade ancestor: not identified.
 */
typedef unsigned char u8; typedef short s16; typedef unsigned short u16;
typedef int s32; typedef float f32;
typedef struct Model {
#ifdef TARGET_PC
    /* PC: id is the low half of the target's handle (an s32 at +0xC), at +0xC on a little-endian host, not +0xE:
     * the view reads the word (docs/port/POINTERS.md 10). */
    u8 pad0[0xC];
    s32 id;
#else
    u8 pad0[0xE];
    s16 id;
#endif
    s16 type;
    u8 pad12[0x50 - 0x12];
    s16 cur;
    u8 pad52[0x58 - 0x52];
    s16 base;
    s16 count;
} Model;
#ifdef TARGET_PC
/* PC: obj is a visual (tVisual, 0x20 bytes: next and func are host pointers), and its data is the target, a
 * level-object node, as a 32-bit window address (docs/port/CARS.md, POINTERS.md 10). */
typedef struct Obj {
    void *next;
    s16 frame;
    u8 pad6[6];
    unsigned int model;
    f32 timer;
} Obj;
#define N64P(x) ((Model *)(uintptr_t)(x))
#else
typedef struct Obj {
    u8 pad0[4];
    s16 frame;
    u8 pad6[6];
    Model *model;
    f32 timer;
} Obj;
#define N64P(x) x
#endif
#ifdef TARGET_PC
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Ent { unsigned int flags, w4; void *obj; f32 scale, w; u16 tex; s16 child, next, mode; void *light, *texture[4], *rect, *clip, *pal; unsigned int prim, env; } Ent;
#else
typedef struct Ent { u8 pad0[0x14]; u16 tex; u8 pad16[0x44 - 0x16]; } Ent;
#endif
#ifdef TARGET_PC
/* PC: a target info record (D_80117530) is 0x40 bytes: its name, file, start function and visual function are
 * pointers. Every field is spelled out, whichever view the loader converts the ROM's records by
 * (docs/port/POINTERS.md 10). */
typedef struct Kind {
    char *name, *file;
    void *start, *visual;
    s16 kind;
    u16 flags;              /* 0x12 */
    s16 shape;
    signed char category, element;
    f32 value;
    s32 sound, w20, w24, w28;
    f32 f2C;
} Kind;
#else
typedef struct Kind { u8 pad0[0x12]; u16 flags; u8 pad14[0x30 - 0x14]; } Kind;
#endif
extern s32 D_801170FC;
extern volatile f32 D_8002EB94;
extern Kind D_80117530[];
extern u16 D_801427C0[];
extern Ent D_8012E700[];
void entity_transform_apply(void *node, s32 unlink);
void entity_spawn_callback(s16 idx, s32 freeChildren, s32 freeSiblings);

void audio_channel_setup(Obj *obj, s16 mode)
{
    Model *m;
    Kind *kind;
    s16 f;
    s16 id;
    u16 tex;

    if (mode == 0) {
apply:
        entity_transform_apply(obj, 1);
        return;
    }
    if (D_801170FC != 0) {
        return;
    }
    m = N64P(obj->model);
    obj->timer -= D_8002EB94;
    if (obj->timer > 0.0f) {
        return;
    }
    obj->frame++;
    obj->timer = 0.0625f;
    if (obj->frame >= m->count) {
        kind = &D_80117530[m->type];
        if (kind->flags & 0x1000) {
            goto apply;
        }
        if (kind->flags & 0x2000) {
            entity_spawn_callback(m->id, 0, 0);
            goto apply;
        }
        obj->frame = 0;
    }
    f = m->base + obj->frame;
    if (f != m->cur) {
        if (D_801427C0[f]) {}
        id = m->id;
        tex = D_801427C0[f];
        D_8012E700[id].tex = tex;
        m->cur = f;
    }
}
