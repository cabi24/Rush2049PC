/* The decompression lock and the cartridge decompressors' entry points (the boot segment's): one decompression at a
 * time (the background loader and the game thread both read waves). The decompressors themselves (LZSS and
 * inflate) are port/ultra's on the PC. */
#include "static_debug_context.h"

/* Makes the decompression lock, a one-message queue, free. */
void init_decompress_lock(void) { decompress_lock_ready=1; osCreateMesgQueue(&decompress_lock_queue,&decompress_lock_message,1); osSendMesg(&decompress_lock_queue,NULL,0); }

/* Takes the decompression lock (making it first if need be): waits for it with block, else returns 0 when it is
 * taken. Returns 1 once it is held. */
s32 acquire_decompress_lock(s32 block) { OSMesg msg; if (!decompress_lock_ready) init_decompress_lock(); if(block) { osRecvMesg(&decompress_lock_queue,&msg,OS_MESG_BLOCK); } else { if(osRecvMesg(&decompress_lock_queue,&msg,OS_MESG_NOBLOCK)==-1) return 0; } return 1; }

/* Gives the decompression lock back. */
void release_decompress_lock(void) {
    osSendMesg(&decompress_lock_queue, NULL, 0);
}

/* Decompresses the LZSS data at cartridge offset src into dst, under the lock. Returns lzss_decode's result. */
s32 lzss_decompress(void* src,void* dst) { s32 result; if(!acquire_decompress_lock(1)) return 0; result=lzss_decode(src,dst); release_decompress_lock(); return result; }

/* Inflates the deflated data at cartridge offset src into dst, under the lock (flag as unzipdata_from_rom takes it).
 * Returns its result. */
s32 inflate_decompress(void* src,void* dst,s32 n) { s32 result; if(!acquire_decompress_lock(1)) return 0; result=unzipdata_from_rom(src,dst,n); release_decompress_lock(); return result; }

