/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef int s32;
typedef short s16;
typedef unsigned char u8;
/* Each pak's open files (paksysdc's tDLList, MAT3 tDLList): head the first. */
typedef struct tDLList {
    u8 indirect, doubly, opaque2[2];
    unsigned int count;
    void **head, **tail;
} tDLList;
extern tDLList gPS_Files[];
extern s16 gPickedPak;

void **pakscr_FindFile(s32 id)
{
    void **node = gPS_Files[gPickedPak].head;

    while (node != 0) {
        u8 *obj = *node;
        if (id == obj[PSF_OFS(17)]) {
            break;
        }
        node = *(void ***)obj;
    }
    return node;
}
