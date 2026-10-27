/* port/assets/tests/test_pak.c: the game's readers of the four controller/pak records D_80144030 on their PC layout
 * (docs/port/POINTERS.md 9): paksys' PakRecord, whose OSPfs at +0x0C holds a pointer, so from there on the record is
 * later than on the N64 and 0x310 bytes long. Each reader must find port 1..3's fields, and the free space and the
 * file entries after the OSPfs: the pak free-space getters (func_800A3598, func_800A35BC), the damaged flag
 * (func_800A35F8), the insert scan (func_800A43FC), the empty-file count (func_800A3518) and the controller's enabled
 * flag (func_800CC848).
 * game: src/blob/func_800A3598.c src/blob/func_800A35BC.c src/blob/func_800A35F8.c src/blob/func_800A43FC.c
 * game: src/blob/func_800A3518.c src/blob/func_800CC848.c
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

PakRecord D_80144030[4];
int8_t D_8011EAE0 = 1;
struct { uint16_t type; uint8_t status, errno_; } D_80149440[4];

int32_t func_800A3598(int32_t port);
uint32_t func_800A35BC(int32_t port);
int8_t func_800A35F8(int32_t port);
void func_800A43FC(void);
int32_t func_800A3518(int32_t port);
int32_t func_800CC848(void *object, int32_t action);
int32_t func_800A1A60(void *node) { return 7; }

int main(void)
{
    struct { uint8_t pad[16]; uint8_t car; } owner = {{0}, 2};
    struct { void *owner; } node = {&owner};
    struct { uint8_t pad[8]; void *node; } obj = {{0}, &node};
    void *pobj = &obj;
    int k;

    D_80144030[2].FreeSpace = 0x1234;
    D_80144030[3].damaged = 1;
    D_80144030[2].inserted = 1;   /* "enabled" in func_800CC848's view */
    CHECK(func_800A3598(2) == 0x1234 && func_800A3598(1) == 0);
    CHECK(func_800A35BC(2) == (0x1234 + 0xFF) >> 8);
    CHECK(func_800A35F8(3) == 1 && func_800A35F8(2) == 0);

    D_80149440[1].status = 1;            /* port 1: a pak in; port 3: taken out */
    D_80144030[3].opaque2 = 1;
    func_800A43FC();
    CHECK(D_80144030[1].opaque2 == 1 && D_80144030[3].opaque2 == 0 && D_80144030[3].opaque3 == 1);
    CHECK(D_80144030[0].opaque2 == 0 && D_80144030[2].opaque3 == 0);

    for (k = 0; k < 16; k++)             /* port 1: files 0..4 hold data (file_size, at entry +8) */
        D_80144030[1].files[k].file_size = k < 5 ? 512 : 0;
    CHECK(func_800A3518(1) == 11 && func_800A3518(0) == 16);

    CHECK(func_800CC848(&pobj, 1) == 7 && func_800CC848(&pobj, 0) == 1);
    owner.car = 1;
    CHECK(func_800CC848(&pobj, 1) == 0);
    return TEST_DONE();
}
