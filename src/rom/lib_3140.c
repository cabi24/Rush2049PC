/* GENERATED ROM-aligned TU — segment 0x3140 (rom/lib_3140)
 * layout map a29a2c188301e27d34ee1904853343fcdf197c4de42725a016014b9cb43b5b7f.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "static_debug_context.h"

/* PROMOTED 2026-10-02 — dma_queue_init
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void dma_queue_init(void) { gDmaInitialized=1; osCreateMesgQueue(&gDmaMessageQueue,&gDmaMessageBuffer,1); osSendMesg(&gDmaMessageQueue,NULL,0); }

/* PROMOTED 2026-10-02 — dma_wait
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 dma_wait(s32 block) { OSMesg msg; if (!gDmaInitialized) dma_queue_init(); if(block) { osRecvMesg(&gDmaMessageQueue,&msg,OS_MESG_BLOCK); } else { if(osRecvMesg(&gDmaMessageQueue,&msg,OS_MESG_NOBLOCK)==-1) return 0; } return 1; }

/* PROMOTED 2026-10-02 — dma_signal
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void dma_signal(void) {
    osSendMesg(&gDmaMessageQueue, NULL, 0);
}

/* PROMOTED 2026-10-02 — lzss_decompress
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 lzss_decompress(void* src,void* dst) { s32 result; if(!dma_wait(1)) return 0; result=lzss_decode(src,dst); dma_signal(); return result; }

/* PROMOTED 2026-10-02 — inflate_decompress
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 inflate_decompress(void* src,void* dst,s32 n) { s32 result; if(!dma_wait(1)) return 0; result=inflate_entry(src,dst,n); dma_signal(); return result; }

