/* fake_init.c: the roster of an attract demo race (MAT3 mdrive.c, fake_init()). */
#include "game/game.h"

/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gPlayers;   /* the first player's record; its first byte is the car slot it watches */
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

/* Set up the attract demo's race on `track`: every car a drone (owner 0, LINK_OWNER_DRONE) in the race (flags
 * 0xB0, LINK_FLAGS_RACER) in slot order on the grid, the other slots empty (owner slot + 6, out of the race). Each
 * demo watches the next car (attractSlot, round the cars): the first player's slot and camera 0's car. The drones
 * catch up (gUseCatchup 2). */
void fake_init(s8 track)
{
    struct { u8 head[0x94]; s8 slot; } *camera0 = (void *) &gCamera[0];
    s16 i;
    s16 place;
    static s16 attractSlot = 0;

    PC_LATCH_CARS() gTrackNum = track;
    gNumCars = DEMO_CARS;
    for (i = 0, place = 0; i < LINK_SLOTS; i++) {
        if (i >= DEMO_CARS) {
            PC_LINK_FLAGS(i) = 0;
            PC_LINK_OWNER(i) = i + 6;
        } else {
            PC_LINK_OWNER(i) = 0;
            PC_LINK_PLACE(i) = place++;
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
