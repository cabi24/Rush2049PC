/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
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
