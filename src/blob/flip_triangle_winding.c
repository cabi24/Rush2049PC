#include "game/game.h"

/* ROM display lists stay in N64 form on PC: 8-byte big-endian commands (docs/port/POINTERS.md). */
#define DLW(w) pc_be32_get(&(w))
#define DLW_SET(w, v) pc_be32_set(&(w), v)
s32 flip_triangle_winding(u32 *gfx) {
    u32 mid;
    u32 low;

    if ((DLW(*gfx) & 0xFF000000) == 0x05000000) {
        mid = DLW(*gfx) & 0xFF00;
        low = DLW(*gfx) & 0xFF;
        DLW_SET(*gfx, (DLW(*gfx) & 0xFFFF0000) | (mid >> 8) | (low << 8));
    } else if ((DLW(*gfx) & 0xFF000000) == 0x07000000 || (DLW(*gfx) & 0xFF000000) == 0x06000000) {
        mid = DLW(*gfx) & 0xFF00;
        low = DLW(*gfx) & 0xFF;
        DLW_SET(*gfx, (DLW(*gfx) & 0xFFFF0000) | (mid >> 8) | (low << 8));
        gfx++;
        mid = DLW(*gfx) & 0xFF00;
        low = DLW(*gfx) & 0xFF;
        DLW_SET(*gfx, (DLW(*gfx) & 0xFFFF0000) | (mid >> 8) | (low << 8));
    }
    return 2;
}
