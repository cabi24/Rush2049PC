/* PS_LockFile: keep a pak file while it is in use (MAT3 paksysdc.c). */
#include "game/game.h"

/* Locks (active set) or unlocks the open file handle. A pak with a locked file keeps its files when it is taken out
 * (PS_PakServer), so a profile in use survives the pak coming out and going back in. Unlocking its last locked file
 * unlocks the pak, and a pak that is not the original one is then forgotten as inserted. */
void PS_LockFile(tPakHandle *handle, s32 active)
{
    s32 pak;
    s32 i;

    if (handle != 0) {
        pak = handle->file->pak;
        if (active != gPaks[pak].files[handle->file->fileNo].locked) {
            gPaks[pak].files[handle->file->fileNo].locked = active;
            if (active) {
                gPaks[pak].locked = 1;
            } else {
                for (i = 0; i < PAK_MAX_FILES; i++) {
                    if (gPaks[pak].files[i].locked) {
                        break;
                    }
                }
                if (i >= PAK_MAX_FILES) {
                    gPaks[pak].locked = 0;
                    if (gPaks[pak].original == 0) {
                        gPaks[pak].inserted = 0;
                    }
                }
            }
        }
    }
}
