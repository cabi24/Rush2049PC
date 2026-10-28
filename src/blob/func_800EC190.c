/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800EC190 (0x800EC190, 224 B): reset the six 8-byte slots of D_80153E88 for `owner` (owner byte, ordinal =
 * running count, flags 0xB0, index 0; the `i >= 6` arm is dead but present in retail: a "slots beyond the active
 * count" branch of a loop whose bound and count are both 6), set the slot count D_801543CA = 6, advance the
 * rotating static counter 0..5 into D_8014A118 and D_80150C04, and set D_80150F14 = 2.
 * D_801163F4 is a function-local static (own .data, 4 zero bytes at 0x801163F4; no other function references it,
 *). D_801543CA is volatile (as in the locked func_80108DA8).
 * DISCLOSED SHAPING DEVICES:
 * 1. compiled-out `if (D_8014A118) {}` before the counter update: the second use that colours
 *     &D_8014A118 into v1.
 * 2. `& 0xFF` on the D_8014A118 store: a no-op for the s8 store. ugen evaluates it into a temp
 *     (`and $8,$2,255; sb $8`), as1 drops the `and` before the byte store, and the temp ring advances by one so the
 *     constant 2 lands in t1 as in retail (ours was t0). Masking the D_80150C04 store instead is equivalent.
 * Provenance: drafts, (static + holder, 2 words), (ring lever).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;

typedef struct Slot {
    s8 owner;       /* 0 */
    u8 pad1[4];
    s8 ordinal;     /* 5 */
    u8 flags;       /* 6 */
    s8 index;       /* 7 */
} Slot;

extern s8 D_8014978C;
extern volatile s16 D_801543CA;
#ifdef TARGET_PC
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
#else
extern Slot D_80153E88[6];
#define PC_LINK_TRACK(i) D_80153E88[i].owner
#define PC_LINK_PLACE(i) D_80153E88[i].ordinal
#define PC_LINK_FLAGS(i) D_80153E88[i].flags
#define PC_LINK_OWNER(i) D_80153E88[i].index
#endif
extern s8 D_8014A118;
extern s8 D_80150C04;
extern s8 D_80150F14;

void func_800EC190(s8 owner)
{
    s16 i;
    s16 n;
    static s16 D_801163F4 = 0;

    D_8014978C = owner;
    D_801543CA = 6;
    for (i = 0, n = 0; i < 6; i++) {
        if (i >= 6) {
            PC_LINK_FLAGS(i) = 0;
            PC_LINK_OWNER(i) = i + 6;
        } else {
            PC_LINK_OWNER(i) = 0;
            PC_LINK_PLACE(i) = n++;
            PC_LINK_TRACK(i) = D_8014978C;
            PC_LINK_FLAGS(i) = 176;
        }
    }
    if (D_8014A118) {}
    D_801163F4++;
    if (D_801163F4 >= 6) {
        D_801163F4 = 0;
    }
    D_8014A118 = D_801163F4 & 0xFF;
    D_80150C04 = D_801163F4;
    D_80150F14 = 2;
}
