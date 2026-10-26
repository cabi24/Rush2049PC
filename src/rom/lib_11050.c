/* GENERATED ROM-aligned TU — segment 0x11050 (rom/lib_11050)
 * layout map d82332505db016bb10f80b0817ff7cfcf886bd3671d9c2ae19aa7abdf6a87e79.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* : hoisted from func_80010628's declarations so that func_80010450, the first slot, sees it. */
typedef struct TransferRequest { void *destination; void *source; unsigned int size; } TransferRequest;

/* PROMOTED 2026-10-12 — func_80010450
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct DmaEntry_80010450 { int length; void *source; unsigned char *destination; } DmaEntry_80010450;
extern TransferRequest D_80037FA8[];
extern volatile unsigned char D_80037FA0;
extern unsigned char D_80037FD8;
extern unsigned char D_80037FE0[];
extern void *D_8003802C;
extern unsigned int D_80038038;
extern unsigned int D_8003803C;
extern unsigned short D_80038028;
extern int osSendMesg(OSMesgQueue *, void *, int);
extern unsigned int func_8002C5E0(unsigned int, unsigned int);
void func_80010450(void)
{
    unsigned char i;
    if (D_80037FD8 != 0) {
        osSendMesg((OSMesgQueue *)D_80037FE0, 0, 0);
    }
    if (D_80037FA0 != 0) {
        for (i = 0; i < D_80037FA0; i++) {
            D_80038038 += 12;
            D_8003803C = func_8002C5E0(D_8003803C, D_80038038);
            ((DmaEntry_80010450 *)D_8003802C)[D_80038028].source = D_80037FA8[i].source;
            ((DmaEntry_80010450 *)D_8003802C)[D_80038028].length = D_80037FA8[i].size;
            ((DmaEntry_80010450 *)D_8003802C)[D_80038028].destination = D_80037FA8[i].destination;
            D_80038028++;
        }
        D_80037FA0 = 0;
        D_80037FD8 = 1;
    } else {
        D_80037FD8 = 0;
    }
}

/* PROMOTED 2026-10-04 — func_800105C4
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, int);
extern unsigned char D_80037FE0[];
extern OSMesg D_80037FF8[];
extern volatile unsigned char D_80037FA0;
extern void (*D_80038020)(void);
extern void func_80010450(void);
void func_800105C4(void)
{
    if (D_80038020 == 0) {
        osCreateMesgQueue((OSMesgQueue *)D_80037FE0, D_80037FF8, 1);
        D_80037FA0 = 0;
        D_80038020 = func_80010450;
    }
}

/* PROMOTED 2026-10-04 — func_8001061C
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void func_8001061C(void)
{
    D_80038020 = 0;
}

/* PROMOTED 2026-10-04 — func_80010628
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern TransferRequest D_80037FA8[];
extern void func_80014594(void);
extern void func_800145DC(void);
extern void func_800105C4(void);
extern void osYieldThread(void);
extern void osInvalDCache(void *, int);
extern int osRecvMesg(OSMesgQueue *, void **, int);
void func_80010628(void *destination, void *source, unsigned int size)
{
    func_80014594();
    func_800105C4();
    if (D_80037FA0 < 4) {
        D_80037FA8[D_80037FA0].destination = destination;
        D_80037FA8[D_80037FA0].source = source;
        D_80037FA8[D_80037FA0].size = (size + 15) & ~15U;
        D_80037FA0++;
        osInvalDCache(destination, size);
        func_800145DC();
        osRecvMesg((OSMesgQueue *) D_80037FE0, 0, 1);
        osYieldThread();
    } else {
        func_800145DC();
    }
}

/* PROMOTED 2026-10-04 — func_80010714
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void func_80010110(void);
void func_80010714(void *destination, void *source, unsigned int size)
{
    func_80014594();
    func_800105C4();
    if (D_80037FA0 < 4) {
        D_80037FA8[D_80037FA0].destination = destination;
        D_80037FA8[D_80037FA0].source = source;
        D_80037FA8[D_80037FA0].size = (size + 15) & ~15U;
        D_80037FA0++;
        osInvalDCache(destination, size);
    }
    func_800145DC();
}

