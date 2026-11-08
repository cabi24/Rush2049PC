/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * AudioEffectPlaying: validate a handle id against the 24-byte table *gSfxInstances
 * (slot = low byte of id).  Returns 0 for id -1 or a stale slot (slot.id != id),
 * 1 if the slot's byte +1 is clear, otherwise whether static-code
 * sound_check(slot.handle) is not -1.  Also matches at -O2.
 * Shaping quirk: the call argument is read through a fresh `e = gSfxInstances + slot`
 * pointer (retail recomputes the address as offset+base into v0); indexing it like
 * the first two reads CSEs the address instead.
 */
typedef signed char s8; typedef unsigned char u8; typedef int s32; typedef unsigned int u32;
typedef struct {
    s8 active;      /* 0 */
    s8 b1;          /* 1 */
    u8 pad2[6];
    s32 id;         /* 8 */
    s32 pad12;
    u32 handle;     /* 16 */
    s32 pad20;
} Entry;            /* 24 */
extern Entry *gSfxInstances;
extern u32 sound_check(u32);
s32 AudioEffectPlaying(s32 id) {
    u8 slot;
    Entry *e;
    if (id != -1) {
        slot = id;
        if (id == gSfxInstances[slot].id) {
            if (gSfxInstances[slot].b1 != 0) {
                e = gSfxInstances + slot;
                return sound_check(e->handle) != -1;
            }
            return 1;
        }
    }
    return 0;
}
