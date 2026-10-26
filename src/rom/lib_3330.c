/* GENERATED ROM-aligned TU — segment 0x3330 (rom/lib_3330)
 * layout map 19814108a381a1fc7b0aa922066738e26c9227e0b9d03980f9b2351dfe682282.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "static_debug_context.h"

/* PROMOTED 2026-10-02 — memchr
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u8 *memchr(u8 *src, u32 c, s32 count) { u8 *p=src; while(count--) { if(*p==c) return p; p++; } return NULL; }

