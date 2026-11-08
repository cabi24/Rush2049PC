/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef int s32;
typedef short s16;
typedef unsigned char u8;
typedef struct Slot {
    void **head;
    s32 pad[3];
} Slot;
extern Slot gPS_Files_head[];
extern s16 gPickedPak;

void **pakscr_FindFile(s32 id)
{
    void **node = gPS_Files_head[gPickedPak].head;

    while (node != 0) {
        u8 *obj = *node;
        if (id == obj[PSF_OFS(17)]) {
            break;
        }
        node = *(void ***)obj;
    }
    return node;
}
