/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned int u32;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef int intptr_t;
#endif
#ifdef TARGET_PC
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9).
 * `value` is the clip-rectangle slot (N64 +0x34), a pointer. */
typedef struct RecordSlot {
    u32 flags, w4;
    void *obj;
    float scale, w;
    unsigned short model;
    short child, next, mode;
    void *light, *tex[4], *rect;
    uintptr_t value;
    void *pal;
    u32 prim, env;
} RecordSlot;
#else
typedef struct RecordSlot {
    unsigned char unknown_00[52];
    u32 value;
    unsigned char unknown_38[12];
} RecordSlot;
#endif
extern RecordSlot D_8012E700[];

/* The third incoming word is homed but not read by the native body. */
void func_800A7830(short index, uintptr_t value, u32 unused)
{
    D_8012E700[index].value = value;
}
