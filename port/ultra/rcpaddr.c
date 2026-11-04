/* port/ultra/rcpaddr.c: addresses past 16 MiB of a larger RDRAM window (game.memory, docs/port/MEMORY.md 3), for the
 * game's display-list relocations and its RAM tests (pc_prelude.h) and for osVirtualToPhysical (memory.c). It needs
 * only pc_rdram_size(), so the tests that build the game's relocations link it with their own window. */
#include "pc_port.h"

/* The RSP takes an address below 0x80000000 as segmented: bits 24-27 the segment, the low 24 bits an offset. On the
 * N64 every physical address fits the offset (8 MiB), so physical is segment 0's offset, and the game's display-list
 * relocations keep 24 bits. A larger window has physical addresses past 16 MiB, which would read as segments 1-15:
 * there the RCP is given the K0 address instead, which the renderer and the audio interpreter take as they take
 * physical. None of this happens at game.memory = original, where no window address reaches 16 MiB. */

/* A K0 address in the window at or past 16 MiB: how such an address reaches the RCP and the game's display lists. */
int pc_rdram_k0_high(uint32_t addr)
{
    return addr >= PC_RDRAM_BASE + PC_RCP_SEGMENT_SPAN && addr - PC_RDRAM_BASE < pc_rdram_size();
}

/* addr (K0, K1 or physical) as an offset in the window; ~0 when it is not one. */
static uint32_t window_offset(uint32_t addr)
{
    uint32_t phys = addr >= 0x80000000u && addr < 0xC0000000u ? addr & 0x1FFFFFFFu : addr;
    return phys < pc_rdram_size() ? phys : ~0u;
}

/* entity_lod_select's and entity_cull_check's relocation of a display-list word, w1 + base: the N64 keeps the
 * offset's 24 bits and puts a segment in bits 24-27 (seg 0-15, or for seg < 0 the one w1 has). Where the result is
 * segment 0 and lies in the window past 16 MiB, its K0 address. A w1 that is such a K0 address already (an earlier
 * relocation, moved by a delta) has no segment of its own. */
uint32_t pc_rcp_reloc(uint32_t w1, uint32_t base, int32_t seg)
{
    uint32_t sum = w1 + base, segbits, phys;
    if (seg >= 0)
        segbits = (uint32_t)(seg & 0xF) << 24;
    else
        segbits = pc_rdram_k0_high(w1) ? 0 : w1 & 0x0F000000u;
    if (segbits == 0 && (phys = window_offset(sum)) != ~0u && phys >= PC_RCP_SEGMENT_SPAN)
        return PC_RDRAM_BASE | phys;
    return (sum & 0x00FFFFFFu) | segbits;
}

