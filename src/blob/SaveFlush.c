/* SaveFlush: write every pak file's changes (MAT3 save.c). */
#include "game/game.h"


/* Writes the changes of every open file that has been read (PS_FlushFile), on every pak; action is the flush's mode
 * (0: a failure asks the player). */
void SaveFlush(s32 action)
{
    s32 i;
    tPakHandle *fileh;

    for (i = 0; i < NUM_PAKS; i++) {
        for (fileh = gPS_Files[i].head; fileh; fileh = fileh->file->next) {
            if (fileh->file->data) {
                PS_FlushFile(fileh, action);
            }
        }
    }
}
