/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * AudioStopAllEffects: mark every active entry of the 24-byte table at *gSfxInstances
 * (count gNumInstances) for refresh (byte +4 = 1), then raise the global flag gStopAllEffects.
 * Arcade ancestor: not identified.
 * Shaping quirk: gStopAllEffects is volatile -- retail stores it through `la t8; sb a3,0(t8)`
 * (as1 hoists the `la` into the blez delay slot); a plain u8 gives `lui at; sb ...,%lo(at)`.
 * The table base is a pointer global (reloaded each iteration because the byte store may alias it),
 * and the count is re-read after each store for the same reason.
 * Also MATCH at -O2. Whole-program unit: EQUAL.
 */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef unsigned short u16;
typedef int s32; typedef unsigned int u32;

typedef struct {
    s8 active;      /* 0 */
    u8 pad1[3];
    u8 flag;        /* 4 */
    u8 pad5[19];
} Entry;            /* 24 */

extern Entry *gSfxInstances;
extern s32 gNumInstances;
extern volatile u8 gStopAllEffects;

void AudioStopAllEffects(void)
{
    s32 i;

    for (i = 0; i < gNumInstances; i++) {
        if (gSfxInstances[i].active) {
            gSfxInstances[i].flag = 1;
        }
    }
    gStopAllEffects = 1;
}
