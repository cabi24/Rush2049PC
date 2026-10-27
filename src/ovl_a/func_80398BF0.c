/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef int s32;
typedef short s16;
typedef unsigned char u8;
#ifndef TARGET_PC
#define PSF_OFS(n) n   /* the pak file record's PC offsets (pc_prelude.h) */
#endif
typedef struct Slot {
    void **head;
    s32 pad[3];
} Slot;
extern Slot D_80144D68[];
extern s16 D_803B9BBA;

void **func_80398BF0(s32 id)
{
    void **node = D_80144D68[D_803B9BBA].head;

    while (node != 0) {
        u8 *obj = *node;
        if (id == obj[PSF_OFS(17)]) {
            break;
        }
        node = *(void ***)obj;
    }
    return node;
}
