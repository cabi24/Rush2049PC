/* GENERATED ROM-aligned TU — segment 0xe4f0 (rom/lib_e4f0)
 * layout map 9fdb0972e66349f97b9135d40fbc75a0c69ad96b11e39f9421f3c0b3ce527617.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-11 — __ull_rshift (ultralib name; formerly labelled __lshrdi3)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips3 -32 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned long long __ull_rshift(unsigned long long a0, unsigned long long a1) {
    return a0 >> a1;
}

/* PROMOTED 2026-10-11 — __ull_rem (ultralib name; formerly labelled __umoddi3)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips3 -32 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned long long __ull_rem(unsigned long long a0, unsigned long long a1) {
    return a0 % a1;
}

/* PROMOTED 2026-10-11 — __ull_div (ultralib name; formerly labelled __udivdi3)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips3 -32 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned long long __ull_div(unsigned long long a0, unsigned long long a1) {
    return a0 / a1;
}

/* PROMOTED 2026-10-11 — __ll_lshift (ultralib name; formerly labelled __ashldi3)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips3 -32 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned long long __ll_lshift(unsigned long long a0, unsigned long long a1) {
    return a0 << a1;
}

/* PROMOTED 2026-10-11 — __ll_rem (ultralib name; formerly labelled __umoddi3_alt)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips3 -32 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
long long __ll_rem(unsigned long long a0, long long a1) {
    return a0 % a1;
}

/* PROMOTED 2026-10-11 — __ll_div (ultralib name; formerly labelled __divdi3)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips3 -32 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
long long __ll_div(long long a0, long long a1) {
    return a0 / a1;
}

/* PROMOTED 2026-10-11 — __ll_mul (ultralib name; formerly labelled __muldi3)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips3 -32 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned long long __ll_mul(unsigned long long a0, unsigned long long a1) {
    return a0 * a1;
}

/* PROMOTED 2026-10-11 — __ull_divremi (ultralib name; formerly labelled __qdivrem)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips3 -32 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __ull_divremi(unsigned long long* div, unsigned long long* rem, unsigned long long a2, unsigned short a3) {
    *div = a2 / a3;
    *rem = a2 % a3;
}

/* PROMOTED 2026-10-11 — __ll_mod (ultralib name; formerly labelled __moddi3)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips3 -32 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
long long __ll_mod(long long a0, long long a1) {
    long long tmp = a0 % a1;

    if ((tmp < 0 && a1 > 0) || (tmp > 0 && a1 < 0)) {
        tmp += a1;
    }
    return tmp;
}

/* PROMOTED 2026-10-11 — __ll_rshift (ultralib name; formerly labelled __ashrdi3)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips3 -32 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
long long __ll_rshift(long long a0, long long a1) {
    return a0 >> a1;
}

#pragma GLOBAL_ASM("asm/us/nonmatchings/rom/lib_e4f0/__osGetCause.s")
