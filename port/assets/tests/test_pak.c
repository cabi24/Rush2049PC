/* port/assets/tests/test_pak.c: the game's readers of the four controller/pak records gPaks on their PC layout
 * (docs/port/POINTERS.md 9): paksys' PakRecord, whose OSPfs at +0x0C holds a pointer, so from there on the record is
 * later than on the N64 and 0x310 bytes long. Each reader must find port 1..3's fields, and the free space and the
 * file entries after the OSPfs: the pak free-space getters (pak_get_free_bytes, PS_GetFreeBlocks), the damaged flag
 * (PS_PakDamaged), the insert scan (pak_poll_inserted), the empty-file count (PS_GetFreeFiles) and the controller's enabled
 * flag (save_file_pak_ready).
 * game: src/blob/pak_get_free_bytes.c src/blob/PS_GetFreeBlocks.c src/blob/PS_PakDamaged.c src/blob/pak_poll_inserted.c
 * game: src/blob/PS_GetFreeFiles.c src/blob/save_file_pak_ready.c
 */
#include <stddef.h>
#include <string.h>

#include "support.h"

typedef struct {   /* libultra's OSPfs, host layout */
    int32_t status;
    void *queue;
    int32_t channel;
    uint8_t id[32], label[32];
    int32_t version, dir_size, inode_table, minode_table, dir_table, inode_start_page;
    uint8_t banks, activebank;
} OSPfs;
typedef struct {
    int8_t flush, flag1, flag2, opaque3;
    uint16_t mapBytes, opaque6;
    uint32_t file_size, game_code;
    uint16_t company_code;
    char ext_name[4], game_name[16];
} PakFile;
typedef struct {
    uint8_t changed;
    int8_t inserted, opaque2, opaque3, opaque4, damaged, opaque6, flushing, flushed, opaque9, opaque10, flush;
    OSPfs pfs;
    int32_t error, FreeSpace;
    uint8_t motorOn, request, level, opaque127;
    int16_t phase;
    uint8_t opaque130[2];
    PakFile files[16];
} PakRecord;
_Static_assert(sizeof(PakRecord) == 0x310, "the record's host size");
_Static_assert(offsetof(PakRecord, files) == 0x90, "the file entries");

PakRecord gPaks[4];
int8_t gPakSysInited = 1;
struct { uint16_t type; uint8_t status, errno_; } cont_status[4];

int32_t pak_get_free_bytes(int32_t port);
uint32_t PS_GetFreeBlocks(int32_t port);
int8_t PS_PakDamaged(int32_t port);
void pak_poll_inserted(void);
int32_t PS_GetFreeFiles(int32_t port);
int32_t save_file_pak_ready(void *object, int32_t action);
int32_t pak_file_unchanged(void *node) { return 7; }

int main(void)
{
    struct { void *next, *prev, *fn[2]; uint8_t pak; } owner = {NULL, NULL, {NULL, NULL}, 2};   /* tPS_File, PC */
    struct { void *owner; } node = {&owner};
    struct { void *next, *prev, *node; } obj = {NULL, NULL, &node};   /* the profile, PC: its file handle */
    void *pobj = &obj;
    int k;

    gPaks[2].FreeSpace = 0x1234;
    gPaks[3].damaged = 1;
    gPaks[2].inserted = 1;   /* "enabled" in save_file_pak_ready's view */
    CHECK(pak_get_free_bytes(2) == 0x1234 && pak_get_free_bytes(1) == 0);
    CHECK(PS_GetFreeBlocks(2) == (0x1234 + 0xFF) >> 8);
    CHECK(PS_PakDamaged(3) == 1 && PS_PakDamaged(2) == 0);

    cont_status[1].status = 1;            /* port 1: a pak in; port 3: taken out */
    gPaks[3].opaque2 = 1;
    pak_poll_inserted();
    CHECK(gPaks[1].opaque2 == 1 && gPaks[3].opaque2 == 0 && gPaks[3].opaque3 == 1);
    CHECK(gPaks[0].opaque2 == 0 && gPaks[2].opaque3 == 0);

    for (k = 0; k < 16; k++)             /* port 1: files 0..4 hold data (file_size, at entry +8) */
        gPaks[1].files[k].file_size = k < 5 ? 512 : 0;
    CHECK(PS_GetFreeFiles(1) == 11 && PS_GetFreeFiles(0) == 16);

    CHECK(save_file_pak_ready(&pobj, 1) == 7 && save_file_pak_ready(&pobj, 0) == 1);
    owner.pak = 1;
    CHECK(save_file_pak_ready(&pobj, 1) == 0);
    return TEST_DONE();
}
