#include "game/game.h"

/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gPlayers;
/* The cameras (152 bytes each; the declaration the others share): the car a camera follows is its byte at +0x94. */
typedef struct Camera { float uv[3][3], position[3]; u8 unknown30[104]; } Camera;
VIEW(Camera[], gCamera);
#define gCamera gCamera_view

/* PC: the attract demo races the setting's cars, all drones (pc_game_types.h, docs/port/MORECARS.md 3.1), latched
 * here at its start; the camera's car (the counter) goes round them all. The loop walks every slot a PC race can
 * use, so the slots after the demo's cars take the loop's own arm for slots past the count, which the N64's 6
 * never reaches: flags 0, not in the race, whatever an earlier race of more cars left in them. */
#define PC_LATCH_CARS() pc_race_cars_begin();
#define DEMO_CARS PC_RACE_CARS
#define LINK_SLOTS PC_MAX_LINKS
void fake_init(s8 owner)
{
    struct { u8 head[0x94]; s8 slot; } *camera0 = (void *) &gCamera[0];
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
    camera0->slot = attractSlot;
    gUseCatchup = 2;
}
