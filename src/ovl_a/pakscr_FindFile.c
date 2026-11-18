/* pakscr_FindFile: the controller-pak manager's file lookup (MAT3 pakscr.c FindFile). */
#include "game/game.h"


/* The handle of the chosen pak's (gPickedPak) open file whose number on the pak is id, NULL if it has none. */
tPakHandle *pakscr_FindFile(s32 id)
{
    tPakHandle *node = gPS_Files[gPickedPak].head;

    while (node != 0) {
        PakFileRecord *file = node->file;
        if (id == file->fileNo) {
            break;
        }
        node = file->next;
    }
    return node;
}
