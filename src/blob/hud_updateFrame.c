/* hud_updateFrame: the HUD's per-frame bookkeeping, a blit animation function (MAT3 hud.c's updateFrame). */
#include "game/game.h"

/* A player's input record as this reads it: its car. */
typedef struct {
    u8 slot;
    char pad1[0x48 - 0x1]; void *ref;
} Pad;

/* The car and model records are the PC's pc_cars and pc_models (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Pad gPlayers[];

/* PC: races of up to PC_MAX_LINKS cars (docs/port/MORECARS.md 2.5, 7). order[] has a place for every car (the N64's
 * 8 hold its 6; AddressSanitizer saw 12 cars write past them), and in a race of more than 6 cars the HUD order
 * dot_order_list (12 entries by the gap), which the minimap's dots read by their AnimID, is cleared whole, so a dot past
 * the race's cars is hidden. A race of 6 clears the N64's 6, as the N64 does. */
#define PC_HUD_ORDER_N PC_MAX_LINKS
#define PC_HUD_CLEAR_N (PC_RACE_CARS > PC_N64_CARS ? PC_MAX_LINKS : 6)

/* Each frame: the four players' race times as digits (TrackTimeStr: a finisher's own time, else the race clock), and
 * the order the minimap draws the cars' dots in (dot_order_list, read from the end): the humans' cars on top, then
 * the drones by place. Always returns 1 (the blit stays). */
s32 hud_updateFrame(s32 blit) {
    s16 order[PC_HUD_ORDER_N];
    s32 k;
    s16 i;
    s16 j;
    s16 idx;
    s16 num;

    for (k = 0; k < 4; k++) {
        cvt_time((PC_CAR_PLACE_LOCKED(k) == 1) ? PC_CAR_SCORE(k) : gModelTime, TrackTimeStr[k], 'f');
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
