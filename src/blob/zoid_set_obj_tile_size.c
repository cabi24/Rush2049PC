/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned int u32;
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
extern RecordSlot ObjInstanceList[];

/* The third incoming word is homed but not read by the native body. */
void zoid_set_obj_tile_size(short index, uintptr_t value, u32 unused)
{
    ObjInstanceList[index].value = value;
}
