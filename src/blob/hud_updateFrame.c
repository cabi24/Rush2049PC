/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/*
 * hud_updateFrame: builds the end-of-race results tables. No callers by jal
 * (reached through a pointer); the argument is unused (homed only).
 *   1. Formats four cars' times into the 8-byte digit buffers at TrackTimeStr
 *      with cvt_time(time, buf, 'f'): the car's own time (+0xF0) when its
 *      finished flag (+0xEF) is 1, else the global gModelTime.
 *   2. Clears the six-entry slot table dot_order_list to -1.
 *   3. order[place] = slot (model record +0x7C6) of the car whose place byte
 *      (car +0xEE) equals that place.
 *   4. Fills dot_order_list from the top down (index num-1-j): first the slot
 *      bytes of the num_humans 0x4C-byte records at gPlayers, then, in place
 *      order, every slot whose model record has +0x7CC == 1, until full.
 *   5. Sets markers_valid = 1 and returns 1.
 * No arcade ancestor identified.
 *
 * Shaping notes:
 *  - gNumActiveCars (car count) is copied to a local once; num_humans is re-read
 *    through a hoisted address after every byte store (`lh 0(t2)`), which is
 *    reproduced by declaring it volatile here (QUIRK: matches the bytes, the
 *    original declaration is unknown).
 *  - The first loop uses its own s32 index; the others share s16 counters.
 *  - `dot_order_list[num - j++ - 1] = idx` (post-increment inside the subscript)
 *    gives retail's store-after-increment order.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef float f32;

typedef struct {
    char pad0[0xEE];
    s8 place;
    s8 finished;
    f32 time;
    char padF4[0x110 - 0xF4]; struct pc_visual visuals[21]; char padF4_308[0x380 - 0x308]; void *input; char padF4_384[0x3B8 - 0x384];
} Car;

typedef struct {
    char pad0[0x7C6];
    s16 slot;
    char pad7C8[4];
    s8 active;
    char pad7CD[0x808 - 0x7CD];
} Model;

typedef struct {
    u8 slot;
    char pad1[0x48 - 0x1]; void *ref;
} Pad;

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Pad gPlayers[];
extern u8 TrackTimeStr[6][8];
extern f32 gModelTime;
extern s8 dot_order_list[];
extern s8 gNumActiveCars;
extern volatile s16 num_humans;
extern s16 markers_valid;

s32 cvt_time(f32 value, u8 *digits, u8 format);

/* PC: races of up to PC_MAX_LINKS cars (docs/port/MORECARS.md 2.5, 7). order[] has a place for every car (the N64's
 * 8 hold its 6; AddressSanitizer saw 12 cars write past them), and in a race of more than 6 cars the HUD order
 * dot_order_list (12 entries by the gap), which the minimap's dots read by their AnimID, is cleared whole, so a dot past
 * the race's cars is hidden. A race of 6 clears the N64's 6, as the N64 does. */
#define PC_HUD_ORDER_N PC_MAX_LINKS
#define PC_HUD_CLEAR_N (PC_RACE_CARS > PC_N64_CARS ? PC_MAX_LINKS : 6)

s32 hud_updateFrame(s32 arg0) {
    s16 order[PC_HUD_ORDER_N];
    s32 k;
    s16 i;
    s16 j;
    s16 idx;
    s16 num;

    for (k = 0; k < 4; k++) {
        cvt_time((PC_CAR_FINISHED(k) == 1) ? PC_CAR_TIME(k) : gModelTime, TrackTimeStr[k], 'f');
    }
    for (i = 0; i < PC_HUD_CLEAR_N; i++) {
        dot_order_list[i] = -1;
    }
    num = gNumActiveCars;
    for (i = 0; i < num; i++) {
        idx = 0;
        for (j = 0; j < num; j++) {
            idx = PC_MODEL_SLOT(j);
            if (i == PC_CAR_PLACE(idx)) {
                break;
            }
        }
        order[i] = idx;
    }
    i = 0;
    for (j = 0; j < num_humans; j++) {
        dot_order_list[num - j - 1] = gPlayers[j].slot;
    }
    for (; i < num; i++) {
        idx = order[i];
        if (PC_MODEL_DRONE_TYPE(idx) == 1) {
            dot_order_list[num - j++ - 1] = idx;
        }
        if (j == num) {
            break;
        }
    }
    markers_valid = 1;
    return 1;
}
