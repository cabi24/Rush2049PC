/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * fake_init (0x800EC190, 224 B): reset the six 8-byte slots of D_80153E88 for `owner` (owner byte, ordinal =
 * running count, flags 0xB0, index 0; the `i >= 6` arm is dead but present in retail: a "slots beyond the active
 * count" branch of a loop whose bound and count are both 6), set the slot count gNumCars = 6, advance the
 * rotating static counter 0..5 into gPlayers and first_camera_slot, and set gUseCatchup = 2.
 * attractSlot is a function-local static (own .data, 4 zero bytes at 0x801163F4; no other function references it,
 *). gNumCars is volatile (as in the locked animRadar).
 * DISCLOSED SHAPING DEVICES:
 * 1. compiled-out `if (gPlayers) {}` before the counter update: the second use that colours
 *     &gPlayers into v1.
 * 2. `& 0xFF` on the gPlayers store: a no-op for the s8 store. ugen evaluates it into a temp
 *     (`and $8,$2,255; sb $8`), as1 drops the `and` before the byte store, and the temp ring advances by one so the
 *     constant 2 lands in t1 as in retail (ours was t0). Masking the first_camera_slot store instead is equivalent.
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

extern s8 gTrackNum;
extern volatile s16 gNumCars;
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gPlayers;
extern s8 first_camera_slot;
extern s8 gUseCatchup;

/* PC: the attract demo races the setting's cars, all drones (pc_game_types.h, docs/port/MORECARS.md 3.1), latched
 * here at its start; the camera's car (the counter) goes round them all. The loop walks every slot a PC race can
 * use, so the slots after the demo's cars take the loop's own arm for slots past the count, which the N64's 6
 * never reaches: flags 0, not in the race, whatever an earlier race of more cars left in them. */
#define PC_LATCH_CARS() pc_race_cars_begin();
#define DEMO_CARS PC_RACE_CARS
#define LINK_SLOTS PC_MAX_LINKS
void fake_init(s8 owner)
{
    s16 i;
    s16 n;
    static s16 attractSlot = 0;

    PC_LATCH_CARS() gTrackNum = owner;
    gNumCars = DEMO_CARS;
    for (i = 0, n = 0; i < LINK_SLOTS; i++) {
        if (i >= DEMO_CARS) {
            PC_LINK_FLAGS(i) = 0;
            PC_LINK_OWNER(i) = i + 6;
        } else {
            PC_LINK_OWNER(i) = 0;
            PC_LINK_PLACE(i) = n++;
            PC_LINK_TRACK(i) = gTrackNum;
            PC_LINK_FLAGS(i) = 176;
        }
    }
    if (gPlayers) {}
    attractSlot++;
    if (attractSlot >= DEMO_CARS) {
        attractSlot = 0;
    }
    gPlayers = attractSlot & 0xFF;
    first_camera_slot = attractSlot;
    gUseCatchup = 2;
}
