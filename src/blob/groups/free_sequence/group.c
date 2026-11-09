#include "game/game.h"

VIEW(s32, gTagMask);
#define gTagMask gTagMask_view

VIEW(s32, gFreeSequences);
#define gFreeSequences gFreeSequences_view

VIEW(s32, gActiveSequences);
#define gActiveSequences gActiveSequences_view

/* PC: a sequence (audiomgr_thread.c's anon_ll_id_3c) has host-sized links: its id is at +0x10 and its free and
 * active flags at +0x14, +0x15 (docs/port/SYNTH.md). A list header keeps its head at +8. */
#define SEQ_ID 0x10
#define SEQ_FREE 0x14
#define SEQ_ACTIVE 0x15

void FreeSequence(s32 *ipa_s0) {
    M2C_FIELD(ipa_s0, s32 *, SEQ_ID) = (s32) (M2C_FIELD(ipa_s0, s32 *, SEQ_ID) & gTagMask);
    if (M2C_FIELD(ipa_s0, s8 *, SEQ_ACTIVE) != 0) {
        RemoveLLE(&gActiveSequences, ipa_s0);
        M2C_FIELD(ipa_s0, s8 *, SEQ_ACTIVE) = 0;
    }
    InsertLLE(&gFreeSequences, ipa_s0, M2C_FIELD(&gFreeSequences, s32 **, 8));
    M2C_FIELD(ipa_s0, s8 *, SEQ_FREE) = 1;
}

/* stand-in caller: keeps FreeSequence out of line under -O3 */
