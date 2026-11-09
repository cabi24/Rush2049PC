/* src/rom/sound/dma_queue.c: the MusyX sound library's queue of cartridge reads for the CPU.
 *
 * Speech and other streams (speech/streams.c) read their compressed data from the cartridge through here. A read is
 * queued (at most four at a time) and joins the hardware layer's next DMA list: sound_dma_flush_requests is the
 * frame hook sound_hw_service_frame calls after it starts a new list. A blocking read then waits for the frame after
 * that, when the list has completed. MusyX is the N64's sound library: no MAT3 counterpart.
 *
 * Still N64-shaped: the sources are 32-bit cartridge addresses, and the list's cache-line bookkeeping counts the
 * N64's 12-byte entries. */
#include "../rom_tu.h"

/* A queued read: where to, from where in the cartridge, how many bytes (rounded up to 16) */
typedef struct TransferRequest { void *destination; void *source; unsigned int size; } TransferRequest;

/* PC: the list's entries are hardware.c's SoundDmaEntryCodebook, 16 bytes (DMA_ENTRY_SIZE): the source is a 32-bit
 * cartridge address and the destination a host pointer (docs/port/MUSYX_DATA.md, docs/port/LAYOUT.md 5). */
typedef struct SoundDmaEntry { int length; unsigned int source; unsigned char *destination; } SoundDmaEntry;
#define DMA_SOURCE(x) ((unsigned int) (uintptr_t) (x))

#define MAX_REQUESTS 4                 /* reads queued for one frame */
#define DMA_ENTRY_N64_BYTES 12          /* a DMA list entry on the N64, which the cache-line bookkeeping counts */

/* The queue and the hardware layer's DMA list */
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
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, int);
extern OSMesg sound_dma_done_message[];
extern void (*sound_dma_flush_hook)(void);
extern void sound_dma_flush_requests(void);
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern void sound_dma_install_queue(void);
extern void osYieldThread(void);
extern void osInvalDCache(void *, int);
extern int osRecvMesg(OSMesgQueue *, void **, int);

/* The frame hook (sound_dma_flush_hook), run as sound_hw_service_frame starts a new DMA list: first wakes the reader
 * waiting on last frame's requests (that list has completed), then adds the requests queued since to the new list
 * and empties the queue; sound_dma_waiting says whether there were any. */
void sound_dma_flush_requests(void)
{
    unsigned char i;
    if (sound_dma_waiting != 0) {
        osSendMesg((OSMesgQueue *)sound_dma_done_queue, 0, OS_MESG_NOBLOCK);
    }
    if (sound_dma_request_count != 0) {
        for (i = 0; i < sound_dma_request_count; i++) {
            sound_dma_list_end += DMA_ENTRY_N64_BYTES;
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

/* Sets the queue up the first time a read asks for it: the done queue, no requests, and the frame hook. */
void sound_dma_install_queue(void)
{
    if (sound_dma_flush_hook == 0) {
        osCreateMesgQueue((OSMesgQueue *)sound_dma_done_queue, sound_dma_done_message, 1);
        sound_dma_request_count = 0;
        sound_dma_flush_hook = sound_dma_flush_requests;
    }
}

/* Takes the frame hook away: queued reads are no longer carried out. */
void sound_dma_remove_queue(void)
{
    sound_dma_flush_hook = 0;
}

/* Reads size bytes from cartridge address source to destination and waits until they have arrived (about two
 * frames). A read the full queue has no room for is dropped, without waiting. */
void sound_dma_read_blocking(void *destination, void *source, unsigned int size)
{
    sound_hw_lock();
    sound_dma_install_queue();
    if (sound_dma_request_count < MAX_REQUESTS) {
        sound_dma_requests[sound_dma_request_count].destination = destination;
        sound_dma_requests[sound_dma_request_count].source = source;
        sound_dma_requests[sound_dma_request_count].size = (size + 15) & ~15U;
        sound_dma_request_count++;
        osInvalDCache(destination, size);
        sound_hw_unlock();
        osRecvMesg((OSMesgQueue *) sound_dma_done_queue, 0, OS_MESG_BLOCK);
        osYieldThread();
    } else {
        /* Original game: with the queue full the read is dropped and the caller, told nothing, goes on without its
         * data. */
        sound_hw_unlock();
    }
}

/* Queues a read of size bytes from cartridge address source to destination and returns at once: the data is there
 * once the next frame's DMA list has run. A read the full queue has no room for is dropped. */
void sound_dma_read_async(void *destination, void *source, unsigned int size)
{
    sound_hw_lock();
    sound_dma_install_queue();
    if (sound_dma_request_count < MAX_REQUESTS) {
        sound_dma_requests[sound_dma_request_count].destination = destination;
        sound_dma_requests[sound_dma_request_count].source = source;
        sound_dma_requests[sound_dma_request_count].size = (size + 15) & ~15U;
        sound_dma_request_count++;
        osInvalDCache(destination, size);
    }
    sound_hw_unlock();
}
