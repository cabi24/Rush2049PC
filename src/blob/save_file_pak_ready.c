/* save_file_pak_ready: whether a profile's pak file can be written (MAT3 save.c). */
#include "game/game.h"

/* A player's profile as this reads it: its pak file's handle (PlayerSaveSlot). */
typedef struct Profile { void *next, *prev; tPakHandle *fileh; } Profile;

/* Whether the profile's file can be used: 1 for a profile without a file (nothing to write), 0 when its pak isn't
 * in, else 1, or with action set whether the file on the pak is still the one read (pak_file_unchanged). */
s32 save_file_pak_ready(Profile **profile, s32 action)
{
    tPakHandle *fileh = (*profile)->fileh;
    s32 pak;
    s32 status = 1;
    if (!fileh) return status;
    pak = fileh->file->pak;
    status = gPaks[pak].inserted;
    if (!status) return 0;
    if (!action) return 1;
    return pak_file_unchanged(fileh);
}
