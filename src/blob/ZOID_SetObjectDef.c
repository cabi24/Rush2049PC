/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/* Complete 44-byte indexed halfword setter at 0x80090770.
 * Native record stride is 0x44; its unsigned halfword is at offset 0x14.
 * The index is signed 16-bit and the value is unsigned 16-bit.
 * No direct arcade equivalent is known. Preserve statement line boundaries.
 */
typedef signed short s16;
typedef unsigned short u16;
typedef unsigned char u8;

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct IndexedRecord44 {
    unsigned int flags, w4;
    void *obj;
    float scale, w;
    u16 value;
    s16 child, next, mode;
    void *light, *tex[4], *rect, *clip, *pal;
    unsigned int prim, env;
} IndexedRecord44;

extern IndexedRecord44 ObjInstanceList[];

void ZOID_SetObjectDef(s16 index, u16 value) {
    ObjInstanceList[index].value = value;
}
