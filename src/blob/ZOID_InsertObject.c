/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 s22; s16 s24; s16 mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Ent;
extern Ent ObjInstanceList[];
extern s16 ObjHead;
s32 ZOID_LastSibling(s32);
void ZOID_InsertObject(s16 a, s16 b) {
    if (b < 0) {
        if (ObjHead < 0) {
            ObjHead = a;
        } else {
            ObjInstanceList[ZOID_LastSibling(ObjHead)].s24 = a;
        }
    } else {
        if (ObjInstanceList[b].s22 == -1) {
            ObjInstanceList[b].s22 = a;
        } else {
            ObjInstanceList[ZOID_LastSibling(ObjInstanceList[b].s22)].s24 = a;
        }
    }
}
