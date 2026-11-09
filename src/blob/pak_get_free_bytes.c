#include "game/game.h"

VIEW(s8, gPaks);
#define gPaks gPaks_view

typedef struct { u8 changed; s8 inserted, b2, b3, b4, damaged, b6, b7, b8, b9, b10, b11; PcPfs pfs; s32 error, FreeSpace;
    u8 rest[0x304 - 0x7C]; } PcPak;
s32 pak_get_free_bytes(s32 arg0) {
    return ((PcPak *) &gPaks)[arg0].FreeSpace;
}
