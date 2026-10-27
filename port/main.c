/* port/main.c: PC entry point (docs/PORT_PLAN.md).
 *
 * For now this only proves the game links. The game's own main (0x800020F0) is compiled as n64_main; the PC
 * layer (Phase 2: threads, message queues, video timing, DMA from the ROM file) will start it. */
#include <stdio.h>

int main(void)
{
    puts("rush2049pc: the game links. Next: the PC layer (docs/PORT_PLAN.md, Phase 2).");
    return 0;
}
