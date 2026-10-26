/* GENERATED ROM-aligned TU — segment 0x8800 (rom/lib_8800)
 * layout map 50222c5fd248f9c10dc181392f4eb6f96365093256559b4f270ae9ee18a2a3d3.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-07-11 — strchr
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
char *strchr(const char *s, int c) {
    const char ch = c;

    while (*s != ch) {
        if (*s == 0) {
            return NULL;
        }
        s++;
    }
    return (char *)s;
}

/* PROMOTED 2026-07-11 — strlen
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u32 strlen(const char *str) {
    const char *p;

    p = str;
    if (*(u8 *)p != 0) {
        do {
            p++;
        } while (*(u8 *)p != 0);
    }
    return (u32)(p - str);
}

/* PROMOTED 2026-07-11 — memcpy
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void *memcpy(void *s1, const void *s2, u32 n) {
    char *su1 = (char *)s1;
    const char *su2 = (const char *)s2;

    while (n > 0) {
        *su1 = *su2;
        su1++;
        su2++;
        n--;
    }
    return (void *)s1;
}

