/* GENERATED ROM-aligned TU — segment 0x11050 (rom/lib_11050)
 * layout map d82332505db016bb10f80b0817ff7cfcf886bd3671d9c2ae19aa7abdf6a87e79.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* : hoisted from sound_dma_read_blocking's declarations so that sound_dma_flush_requests, the first slot, sees it. */
typedef struct TransferRequest { void *destination; void *source; unsigned int size; } TransferRequest;

/* PROMOTED 2026-10-12 — sound_dma_flush_requests
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* PC: the list's entries are hardware.c's SoundDmaEntryCodebook, 16 bytes (DMA_ENTRY_SIZE): the source is a 32-bit
 * cartridge address and the destination a host pointer (docs/port/MUSYX_DATA.md, docs/port/LAYOUT.md 5). */
typedef struct SoundDmaEntry { int length; unsigned int source; unsigned char *destination; } SoundDmaEntry;
#define DMA_SOURCE(x) ((unsigned int) (uintptr_t) (x))
extern TransferRequest sound_dma_requests[];
extern volatile unsigned char sound_dma_request_count;
extern unsigned char sound_dma_waiting;
extern unsigned char sound_dma_done_queue[];
extern void *sound_dma_list;
extern unsigned int sound_dma_list_end;
extern unsigned int sound_dma_list_cache_cursor;
extern unsigned short sound_dma_list_count;
extern int osSendMesg(OSMesgQueue *, void *, int);
extern unsigned int dcache_create_dirty_lines(unsigned int, unsigned int);
void sound_dma_flush_requests(void)
{
    unsigned char i;
    if (sound_dma_waiting != 0) {
        osSendMesg((OSMesgQueue *)sound_dma_done_queue, 0, 0);
    }
    if (sound_dma_request_count != 0) {
        for (i = 0; i < sound_dma_request_count; i++) {
            sound_dma_list_end += 12;
            sound_dma_list_cache_cursor = dcache_create_dirty_lines(sound_dma_list_cache_cursor, sound_dma_list_end);
            ((SoundDmaEntry *)sound_dma_list)[sound_dma_list_count].source = DMA_SOURCE(sound_dma_requests[i].source);
            ((SoundDmaEntry *)sound_dma_list)[sound_dma_list_count].length = sound_dma_requests[i].size;
            ((SoundDmaEntry *)sound_dma_list)[sound_dma_list_count].destination = sound_dma_requests[i].destination;
            sound_dma_list_count++;
        }
        sound_dma_request_count = 0;
        sound_dma_waiting = 1;
    } else {
        sound_dma_waiting = 0;
    }
}

/* PROMOTED 2026-10-04 — sound_dma_install_queue
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, int);
extern unsigned char sound_dma_done_queue[];
extern OSMesg sound_dma_done_message[];
extern volatile unsigned char sound_dma_request_count;
extern void (*sound_dma_flush_hook)(void);
extern void sound_dma_flush_requests(void);
void sound_dma_install_queue(void)
{
    if (sound_dma_flush_hook == 0) {
        osCreateMesgQueue((OSMesgQueue *)sound_dma_done_queue, sound_dma_done_message, 1);
        sound_dma_request_count = 0;
        sound_dma_flush_hook = sound_dma_flush_requests;
    }
}

/* PROMOTED 2026-10-04 — sound_dma_remove_queue
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_dma_remove_queue(void)
{
    sound_dma_flush_hook = 0;
}

/* PROMOTED 2026-10-04 — sound_dma_read_blocking
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern TransferRequest sound_dma_requests[];
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern void sound_dma_install_queue(void);
extern void osYieldThread(void);
extern void osInvalDCache(void *, int);
extern int osRecvMesg(OSMesgQueue *, void **, int);
void sound_dma_read_blocking(void *destination, void *source, unsigned int size)
{
    sound_hw_lock();
    sound_dma_install_queue();
    if (sound_dma_request_count < 4) {
        sound_dma_requests[sound_dma_request_count].destination = destination;
        sound_dma_requests[sound_dma_request_count].source = source;
        sound_dma_requests[sound_dma_request_count].size = (size + 15) & ~15U;
        sound_dma_request_count++;
        osInvalDCache(destination, size);
        sound_hw_unlock();
        osRecvMesg((OSMesgQueue *) sound_dma_done_queue, 0, 1);
        osYieldThread();
    } else {
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-04 — sound_dma_read_async
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void osYieldThread(void);
void sound_dma_read_async(void *destination, void *source, unsigned int size)
{
    sound_hw_lock();
    sound_dma_install_queue();
    if (sound_dma_request_count < 4) {
        sound_dma_requests[sound_dma_request_count].destination = destination;
        sound_dma_requests[sound_dma_request_count].source = source;
        sound_dma_requests[sound_dma_request_count].size = (size + 15) & ~15U;
        sound_dma_request_count++;
        osInvalDCache(destination, size);
    }
    sound_hw_unlock();
}

