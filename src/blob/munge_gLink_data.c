/* munge_gLink_data.c: the race's cars from the roster (MAT3 mdrive.c, munge_gLink_data()).
 *
 * At the race start the roster (gLink, pc_links on the PC: who drives each car slot, and its start place) is turned
 * into the model and car records' race fields and the lists of active cars, drones and humans the simulation and
 * the drones' driving walk. */
#include "game/game.h"


#define MAX_LINKS 6             /* MAT3 LINK_OWNER_HUMAN: an owner below it is a drone */
#define LINK_ACTIVE 0x80        /* MAT3 LINK_FLAG_IN_GAME: the slot is in the race */

/* The model record (tModelData), the race fields. */
typedef struct tModelData {
    /* 0x000 */ u8 pad0[0x7C6];
    /* 0x7C6 */ s16 slot;
    /* 0x7C8 */ s16 in_game;
    /* 0x7CA */ s16 we_control;     /* this machine simulates it (its owner is 0, LINK_OWNER_DRONE) */
    /* 0x7CC */ s8 drone_type;      /* DRONE_TYPE_* */
    /* 0x7CD */ u8 pad7CD[0x3B];
} tModelData; /* 0x808 */

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the race fields. */
typedef struct {
    /* 0x000 */ u8 pad0[0xEE];
    /* 0x0EE */ u8 place;           /* the start place */
    /* 0x0EF */ u8 padEF[0x110 - 0xEF]; struct pc_visual visuals[21]; u8 padEF_308[0x35B - 0x308];
    /* 0x35B */ s8 slot;
    /* 0x35C */ u8 pad35C[0x380 - 0x35C]; void *input; u8 pad35C_384[0x3B8 - 0x384];
} tCarData; /* 0x3B8 */

/* For each of the gNumCars slots: whether this machine simulates it, and if the roster has it in the race, its
 * slot and start place, its kind (drone or human) and its place in the model records' list of active slots
 * (gNumActiveCars of them, PC_MODEL_SLOT(n)). The slots past gNumCars are out of the race. Then the lists: humans[]
 * (num_humans), the drones (PC_DRONE, num_drones) and those this machine drives (PC_OUR_DRONE, num_our_drones).
 * The model iteration count starts again at 0. */
void munge_gLink_data(void) {
    s32 i;
    s32 index;
    s32 j;
    tModelData *m;
    tCarData *car;

    gNumActiveCars = 0;
    for (i = 0; i < gNumCars; i++) {
        m = PC_MODEL(i);
        car = PC_CAR(i);
        m->we_control = (PC_LINK_OWNER(i) == 0);
        m->drone_type = DRONE_TYPE_NONE;
        m->in_game = (PC_LINK_FLAGS(i) & LINK_ACTIVE) != 0;
        if (m->in_game) {
            PC_MODEL_SLOT(gNumActiveCars++) = i;
            car->slot = i;
            car->place = PC_LINK_PLACE(i);
            m->drone_type = (PC_LINK_OWNER(i) < MAX_LINKS) ? DRONE_TYPE_AI : DRONE_TYPE_HUMAN;
        }
    }
    for (i = gNumCars; i < PC_MODEL_SLOTS; i++) {
        m = PC_MODEL(i);
        m->we_control = (PC_LINK_OWNER(i) == 0);
        m->drone_type = DRONE_TYPE_NONE;
        m->in_game = 0;
    }
    num_our_drones = 0;
    j = num_our_drones;
    num_drones = j;
    num_humans = j;
    for (i = 0; i < gNumActiveCars; i++) {
        index = PC_MODEL_SLOT(i);
        if (PC_MODEL_DRONE_TYPE(index) == DRONE_TYPE_HUMAN) {
            j = num_humans;
            humans[j] = index;
            num_humans = j + 1;
        } else {
            j = num_drones;
            PC_DRONE(j) = index;
            num_drones = j + 1;
            if (PC_MODEL_WE_CONTROL(index)) {
                PC_OUR_DRONE(num_our_drones++) = index;
            }
        }
    }
    gModelIteration = 0;
}
