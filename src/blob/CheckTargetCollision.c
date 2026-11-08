/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (identical words at -O2) */
/*
 * NOT STRICT: code identical, own jump table unverified by the scorer.
 * Checked by hand: the object's .rodata is six R_MIPS_32 .text entries
 * +0x2d8 x3, +0x2e8 x3 = 0x800BED78 x3, 0x800BED88 x3, equal to the retail
 * words at 0x80123E5C..0x80123E70 that the code references.
 *
 * Historical label CheckTargetCollision is misleading.  This is the per-car object
 * collision pass: (1) when the mode word gRace is 6, walk the transient
 * object list gAlwaysColList.active, test each object with its shape callback
 * gTargetCollFuncPtrs[type->shape] and remove hits from the pool (FreeStatLLE);
 * (2) descend the 28-byte quadtree at gTargetGroups with the car position and
 * test every object of the leaf (gTargetGroupList[first + i]).  Shapes 0-2 are
 * called as (&player, obj+0x44, obj+0x54, 0), shapes 3-5 as (obj, &player,
 * 0, 0).  A hit sets flag 4, records the player and calls type->onHit.
 *
 * Shaping facts:
 * - the box test must enclose the split test (`if (inside) { if (flags & 1)
 *   {...} else break; } else return;`); early-return form reloads pos[] (19
 *   aligned rows);
 * - `gTargetGroups[tree->child2].maxz/.maxx` directly (a named `sub` pointer
 *   swaps the addu operands: 1 word);
 * - unusedA/unusedB are frame filler: 24 bytes above and 36 bytes below
 *   pos[] are never touched by retail (frame 136).  Their real declarations
 *   are unknown (not the original source).
 */
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned int u32;
typedef int s32;
typedef float f32;

typedef struct Object {
    char pad0[4];
    u8 flags;
    char pad5[11];
    s16 type;
    char pad12[50];
    f32 unk44[4];
    f32 unk54[2];
    s8 player;
} Object;

typedef s32 (*HitFunc)(void *, void *, void *, s32);

/* PC: a target info record (targetInfo) is 0x40 bytes: its name, file, start function and visual function are
 * pointers. Every field is spelled out, whichever view the loader converts the ROM's records by
 * (docs/port/POINTERS.md 10). */
typedef struct ObjType {
    char *name, *file;
    void (*onHit)(Object *);
    void *visual;
    s16 shape;              /* 0x10 */
    u16 flags;
    s16 id;
    s8 category, element;
    f32 value;
    s32 sound, w20, w24, w28;
    f32 f2C;
} ObjType;

typedef struct Node {
    struct Node *next;
    Object *obj;
} Node;

/* PC: a StatLL list header's host layout, as every view has it (docs/port/POINTERS.md 9): doubly flag, element
 * count and size, then the element array, the active list and the free list pointer-sized (40 bytes). */
typedef struct Pool {
    u8 doubly;
    int count, size;
    void *mem;
    Node *active;
    Node *free;
} Pool;

typedef struct Car {
    char pad0[8];
    f32 pos[3];
    char pad14[0x110 - 0x14]; struct pc_visual visuals[21]; char pad14_308[0x380 - 0x308]; void *input; char pad14_384[0x3B8 - 0x384];
} Car;

typedef struct Tree {
    char pad0[3];
    u8 flags;
    f32 minx;
    f32 maxx;
    f32 minz;
    f32 maxz;
    u16 count;
    u16 first;
    u16 child2;
    u16 child3;
} Tree;

extern s32 gGameState;
extern s32 gRace;
extern s8 expansion_pak_present;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Pool gAlwaysColList;
extern ObjType targetInfo[];
extern HitFunc gTargetCollFuncPtrs[];
extern Tree *gTargetGroups;
extern Object **gTargetGroupList;
extern void FreeStatLLE(Pool *, Node *);

void CheckTargetCollision(s16 player) {
    s32 unusedA[6];
    f32 pos[3];
    s32 unusedB[9];
    Car *car;
    Node *node; Node *next; Object *obj; ObjType *type; Tree *tree; s32 idx; s32 i; s32 shape; void *a0; void *a1; void *a2; f32 x; f32 z;

    car = PC_CAR(player);
    if ((gGameState & 8) && expansion_pak_present == 0) {
        return;
    }
    if (gRace == 6) {
        for (node = gAlwaysColList.active; node != 0; node = next) {
            obj = node->obj;
            next = node->next;
            type = &targetInfo[obj->type];
            if (gTargetCollFuncPtrs[type->shape](&player, obj->unk44, obj->unk54, 0)) {
                obj->flags |= 4;
                obj->player = player;
                if (type->onHit != 0) {
                    type->onHit(obj);
                }
                FreeStatLLE(&gAlwaysColList, node);
            }
        }
    }
    pos[0] = car->pos[0];
    pos[1] = car->pos[1];
    pos[2] = car->pos[2];
    idx = 0;
    for (;;) {
        tree = &gTargetGroups[idx];
        if (tree->minx <= pos[0] && pos[0] < tree->maxx && tree->minz <= pos[2] && pos[2] < tree->maxz) {
            if (tree->flags & 1) {
        z = gTargetGroups[tree->child2].maxz;
        x = gTargetGroups[tree->child2].maxx;
        if (z <= pos[2]) {
            if (pos[0] < x) {
                idx = tree->count;
            } else {
                idx = tree->first;
            }
        } else {
            if (pos[0] < x) {
                idx = tree->child2;
            } else {
                idx = tree->child3;
            }
        }
            } else {
                break;
            }
        } else {
            return;
        }
    }
    for (i = 0; i < tree->count; i++) {
        obj = gTargetGroupList[tree->first + i];
        if (obj != 0 && (obj->flags & 2) && (obj->flags & 8)) {
            type = &targetInfo[obj->type];
            shape = type->shape;
            switch (shape) {
                case 0:
                case 1:
                case 2:
                    a0 = &player;
                    a1 = obj->unk44;
                    a2 = obj->unk54;
                    break;
                case 3:
                case 4:
                case 5:
                    a0 = obj;
                    a1 = &player;
                    a2 = 0;
                    break;
                default:
                    continue;
            }
            if (gTargetCollFuncPtrs[shape](a0, a1, a2, 0)) {
                obj->flags |= 4;
                obj->player = player;
                if (type->onHit != 0) {
                    type->onHit(obj);
                }
            }
        }
    }
}
