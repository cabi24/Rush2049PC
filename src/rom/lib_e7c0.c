/* GENERATED ROM-aligned TU — segment 0xe7c0 (rom/lib_e7c0)
 * layout map 0a6559b9e67a367eae3e2798ff28a2cda0fa0eac96f868fb59552d670bc56f6c.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-01 — __osPiCreateAccessQueue (ultralib name; formerly labelled osPiInit)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __osPiCreateAccessQueue(void) {
    __osPiInitialized = 1;
    osCreateMesgQueue(&__osPiMesgQueue, &__osPiMesg, 1);
    osSendMesg(&__osPiMesgQueue, NULL, 0);
}

/* PROMOTED 2026-10-01 — __osPiGetAccess (formerly labelled osPiGetAccess)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __osPiGetAccess(void) {
    void *sp1C;

    if (__osPiInitialized == 0) {
        __osPiCreateAccessQueue();
    }
    osRecvMesg(&__osPiMesgQueue, &sp1C, 1);
}

/* PROMOTED 2026-10-01 — __osPiRelAccess (formerly labelled osPiReleaseAccess)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __osPiRelAccess(void) {
    osSendMesg(&__osPiMesgQueue, NULL, 0);
}

/* PROMOTED 2026-10-01 — __osPiRawReadIo (formerly labelled osPiReadWord)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 __osPiRawReadIo(u32 devAddr, u32* data) {
    register u32 stat;


    

    while ((stat = *(volatile u32*)0xA4600010) & 3);
    *data = (*(volatile u32*)((u32)osRomBase | devAddr | 0xA0000000));

    return 0;
}

