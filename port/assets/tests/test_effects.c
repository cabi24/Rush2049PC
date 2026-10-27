/* port/assets/tests/test_effects.c: game code that walks game data up to the address of the symbol after it
 * (docs/port/SYNTH.md 3). On the N64 the next symbol is there; on the PC a symbol with pointers has storage of its
 * own (port/data), so a walk bounded by its address never meets it. effect_system_init (the stunt and battle
 * modes' race start) clears the four 0x7C-byte player records D_801569B8 up to &D_80156BA8, a Gfx *: on the
 * synthetic cartridge it cleared the arena to the RDRAM window's end and faulted there. Here the records have a
 * guard after them and D_80156BA8 is elsewhere, as in the game: the walk must stop at the records' end.
 * game: src/blob/effect_system_init.c
 */
#include <stddef.h>
#include <string.h>

#include "support.h"

#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)

#define RECORDS (4 * 0x7C)
static struct {
    uint8_t records[RECORDS];
    uint8_t guard[256];   /* where the walk went on */
} g_players __attribute__((aligned(16)));
ALIAS(D_801569B8, g_players, 0);
void *D_80156BA8 = (void *)0x1234;   /* the Gfx * after the records on the N64; own storage here */
uint8_t D_80152038[0x1E0];
float D_8016139C = 1.0f;
int32_t D_80152738 = 1;

void *n64_memset(void *p, int c, uint32_t n) { return memset(p, c, n); }

void effect_system_init(void);

int main(void)
{
    memset(&g_players, 0x5A, sizeof g_players);
    memset(D_80152038, 0x5A, sizeof D_80152038);
    effect_system_init();

    int records_ok = 1, guard_ok = 1, rest_ok = 1;
    for (int i = 0; i < 4; i++) {
        const uint8_t *r = g_players.records + 0x7C * i;
        int32_t flags;
        memcpy(&flags, r, 4);
        records_ok &= flags == 0x15000000;   /* each record cleared, with its first word's flag set */
        for (int k = 4; k < 0x7C; k++)
            records_ok &= r[k] == 0;
    }
    for (size_t k = 0; k < sizeof g_players.guard; k++)
        guard_ok &= g_players.guard[k] == 0x5A;
    for (size_t k = 0; k < sizeof D_80152038; k++)
        rest_ok &= D_80152038[k] == 0;
    CHECK(records_ok);
    CHECK(guard_ok);                          /* nothing written past the records */
    CHECK(D_80156BA8 == (void *)0x1234);
    CHECK(rest_ok);
    CHECK(D_8016139C == 0.0f && D_80152738 == 0);
    if (!test_failures)
        printf("effect_system_init: the four player records cleared, nothing after them\n");
    return TEST_DONE();
}
