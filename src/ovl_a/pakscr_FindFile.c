#include "game/game.h"

/* Each pak's open files (paksysdc's tDLList, MAT3 tDLList): head the first. */
typedef struct tDLList {
    u8 indirect, doubly, opaque2[2];
    unsigned int count;
    void **head, **tail;
} tDLList;
VIEW(tDLList[], gPS_Files);
#define gPS_Files gPS_Files_view

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
