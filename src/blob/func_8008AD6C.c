/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* func_8008AD6C: flip the winding of one F3DEX2 triangle command in a display list. For opcode 0x05
 * (G_TRI1) it swaps the last two vertex-index bytes of word 0; for 0x06 (G_TRI2) and 0x07 (G_QUAD) it swaps them in
 * both words. Returns 2 (the command's word count). N64-only, no arcade ancestor.
 * Shape: every read goes through `*gfx` (no word local) and the opcode is tested as three separate
 * `(*gfx & 0xFF000000)` expressions. A `w = *gfx` local or an `op` variable makes uopt propagate the byte
 * pieces into ugen temps (29 words); reading memory keeps `mid`/`low` as coloured webs (v1/a1) and
 * the load in v0 as in retail. No shaping devices, no unused locals. */
typedef unsigned int u32;
typedef int s32;

#ifdef TARGET_PC
/* ROM display lists stay in N64 form on PC: 8-byte big-endian commands (docs/port/POINTERS.md). */
#define DLW(w) pc_be32_get(&(w))
#define DLW_SET(w, v) pc_be32_set(&(w), v)
#else
#define DLW(w) w
#define DLW_SET(w, v) w = v
#endif
s32 func_8008AD6C(u32 *gfx) {
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
