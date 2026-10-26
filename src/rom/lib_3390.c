/* GENERATED ROM-aligned TU — segment 0x3390 (rom/lib_3390)
 * layout map c7ac6d56b89794475c018024482797a9cf040f0a2f966b6830aa3981c391cf0e.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "static_debug_context.h"

/* PROMOTED 2026-10-02 — memset
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void* memset(register void* dest, register int c, register unsigned n) {
    unsigned char* p = dest;
    int alignment;
    unsigned* words;
    if (c == 0) {
        alignment = (unsigned)p & 3;
        while(n && alignment > 0 && alignment != 4) {
            *p++ = 0;
            alignment++;
            n--;
        }
        words = (unsigned*)p;
        while(n >= 4) { *words++ = 0; n -= 4; }
        p = (unsigned char*)words;
    }
    while(n-- > 0) *p++ = c;
    return dest;
}

