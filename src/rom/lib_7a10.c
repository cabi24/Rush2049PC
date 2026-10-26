/* GENERATED ROM-aligned TU — segment 0x7a10 (rom/lib_7a10)
 * layout map 8b1fd51a1f60dea4ed88cec2a86189a75ace38461fdb46a2f303d742c3882a76.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-01 — osSetEventMesg (formerly labelled osSetEventMesgAlt)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void osSetEventMesg(OSEvent event, OSMesgQueue* mq, OSMesg msg) {
    register u32 saveMask;
    __OSEventState* es;


    saveMask = __osDisableInt();

    es = &__osEventStateTab[event];

    es->messageQueue = mq;
    es->message = msg;

    if (event == OS_EVENT_PRENMI) {
        if (__osShutdown && !gEventTypeFlag) {
            osSendMesg(mq, msg, OS_MESG_NOBLOCK);
        }
        gEventTypeFlag = TRUE;
    }

    __osRestoreInt(saveMask);
}

