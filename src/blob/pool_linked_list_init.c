/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
#ifdef TARGET_PC
/* PC: the header's host layout (docs/port/POINTERS.md 9), with the n64 flag of a list whose elements keep their
 * N64 layout and are linked by 32-bit window addresses (the level-object nodes, POINTERS.md 10). */
typedef struct { u8 doubly; u8 n64; u8 pad2[2]; s32 count; s32 size; u8 *mem; intptr_t f16; u8 *free; } Pool;
void pool_linked_list_init(Pool *p) {
    u8 *m = p->mem;
    s32 i;
    p->f16 = 0;
    p->free = m;
    if (m != 0) {
        for (i = 0; i < p->count - 1; i++) {
            pc_statll_set_link(p->n64, m + i * p->size, 0, m + (i + 1) * p->size);
        }
        pc_statll_set_link(p->n64, m + i * p->size, 0, 0);
    }
}
#else
typedef struct { u8 pad0[4]; s32 count; s32 size; u8 *mem; intptr_t f16; u8 *free; } Pool;
void pool_linked_list_init(Pool *p) {
    u8 *m = p->mem;
    s32 i;
    p->f16 = 0;
    p->free = m;
    if (m != 0) {
        for (i = 0; i < p->count - 1; i++) {
            *(u8 **)(m + i * p->size) = m + (i + 1) * p->size;
        }
        *(u8 **)(m + i * p->size) = 0;
    }
}
#endif
