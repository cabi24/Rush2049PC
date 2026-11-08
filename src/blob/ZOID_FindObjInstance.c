/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 key; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Ent;
typedef struct { u8 *base; s32 x; } Tab;
extern s32 gNumObjects;
extern Ent ObjInstanceList[];
extern Tab gRomObjDefPtrs[];
s32 game_strcmp(void *, void *);
s16 ZOID_FindObjInstance(void *a) {
    s32 i;
    Ent *e = ObjInstanceList;
    for (i = 0; i < gNumObjects; i++, e++) {
        if (game_strcmp(a, gRomObjDefPtrs[e->key >> 10].base + (e->key & 0x3FF) * 88) == 0) {
            return i;
        }
    }
    return -1;
}
