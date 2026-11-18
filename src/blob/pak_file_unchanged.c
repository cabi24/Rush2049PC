/* pak_file_unchanged: whether a pak file is still the one read (MAT3 paksysdc.c's _FileChanged, inverted). */
#include "game/game.h"

/* Whether the pak is in. */
static s32 pak_enabled(tPak *pak)
{
    return pak->inserted;
}

/* Makes the serial bus lock the first time it is needed (a copy of pfs_queue_init). */
static void pak_queue_init(void)
{
    if (!si_lock_inited) {
        si_lock_inited = 1; queue_init(&si_lock_queue, &si_lock_msg, 1);
        lock_give(&si_lock_queue);
    }
}

/* Takes the serial bus lock (a copy of si_lock). */
static void pak_lock(void)
{
    pak_queue_init();
    lock_take(&si_lock_queue);
}

/* 1 when the file's directory entry on the pak is still the one the pak system read (so no other console wrote it
 * meanwhile), 0 when it differs, can't be read or the pak is out. */
int pak_file_unchanged(tPakHandle *handle)
{
    tPak *pak;
    u32 port,file;
    int error;
    OSPfsState state;
    port = handle->file->pak;
    file = handle->file->fileNo;
    pak = &gPaks[port];
    if (!pak_enabled(pak)) return 0;
    pak_lock();
    error = osPfsFileState(&pak->pfs,file,&state);
    if (error) {}
    si_unlock();
    if (error) return 0;
    if (game_memcmp(&state,&gPaks[port].files[file].state,32)) return 0;
    return 1;
}
