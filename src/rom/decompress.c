/* GENERATED ROM-aligned TU — segment 0x3140 (rom/lib_3140)
 * layout map a29a2c188301e27d34ee1904853343fcdf197c4de42725a016014b9cb43b5b7f.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "static_debug_context.h"

/* PROMOTED 2026-10-02 — init_decompress_lock
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void init_decompress_lock(void) { decompress_lock_ready=1; osCreateMesgQueue(&decompress_lock_queue,&decompress_lock_message,1); osSendMesg(&decompress_lock_queue,NULL,0); }

/* PROMOTED 2026-10-02 — acquire_decompress_lock
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 acquire_decompress_lock(s32 block) { OSMesg msg; if (!decompress_lock_ready) init_decompress_lock(); if(block) { osRecvMesg(&decompress_lock_queue,&msg,OS_MESG_BLOCK); } else { if(osRecvMesg(&decompress_lock_queue,&msg,OS_MESG_NOBLOCK)==-1) return 0; } return 1; }

/* PROMOTED 2026-10-02 — release_decompress_lock
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void release_decompress_lock(void) {
    osSendMesg(&decompress_lock_queue, NULL, 0);
}

/* PROMOTED 2026-10-02 — lzss_decompress
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 lzss_decompress(void* src,void* dst) { s32 result; if(!acquire_decompress_lock(1)) return 0; result=lzss_decode(src,dst); release_decompress_lock(); return result; }

/* PROMOTED 2026-10-02 — inflate_decompress
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 inflate_decompress(void* src,void* dst,s32 n) { s32 result; if(!acquire_decompress_lock(1)) return 0; result=unzipdata_from_rom(src,dst,n); release_decompress_lock(); return result; }

