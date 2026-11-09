#include "game/game.h"

typedef struct Player {
    /* 0 */ u8 pad0;
    /* 1 */ u8 id;
    /* 2 */ u8 pad2[74];
} Player; /* 0x4C */

VIEW(Player[], gCircuitCars);
#define gCircuitCars gCircuitCars_view
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

#define PC_LATCH_CARS() if (!(gGameState & 0x2000000)) pc_race_cars_begin();
#define RACE_CARS (gRace == 3 ? PC_N64_CARS : PC_RACE_CARS)
#define LINK_SLOTS PC_MAX_LINKS

void CalcPolePositions(void)
{
    s32 extra;
    s32 slot;
    s32 prior;
    u8 next = 0;
    PC_LINK *config;

    PC_LATCH_CARS() extra = gNumDrones;
    if (RACE_CARS - gNumPlayers < extra) {
        extra = RACE_CARS - gNumPlayers;
    }
    if (gRace == 4 || gRace == 5 || gRace == 6) {
        extra = 0;
    }
    gNumCars = gNumPlayers + extra;
    for (slot = 0, config = PC_LINKS; slot < LINK_SLOTS; slot++, config++) {
        if (gRace == 3 && slot < gNumCars && gCircuitTrack > 0) {
            PC_LINKP_PLACE(config) = gCircuitCars[(slot == 0) ? gCircuitPlayer : ((slot == 1) ? gCircuitPlayer ^ 1 : slot)].id;
            PC_LINKP_FLAGS(config) = 176;
            if (slot < gNumPlayers) {
                PC_LINKP_OWNER(config) = 6;
            } else {
                PC_LINKP_OWNER(config) = 0;
            }
        } else if (slot < gNumPlayers) {
            PC_LINKP_PLACE(config) = extra++;
            PC_LINKP_OWNER(config) = 6;
            PC_LINKP_FLAGS(config) = 176;
        } else if (slot < gNumCars) {
            if (gRace == 3) {
                while (1) {
                    PC_LINKP_PLACE(config) = Random(gNumCars);
                    for (prior = 0; prior < slot; prior++) {
                        if (PC_LINKP_PLACE(config) == PC_LINK_PLACE(prior)) {
                            break;
                        }
                    }
                    if (slot == prior) {
                        break;
                    }
                }
            } else {
                PC_LINKP_PLACE(config) = next++;
            }
            PC_LINKP_OWNER(config) = 0;
            PC_LINKP_FLAGS(config) = 176;
        } else {
            PC_LINKP_OWNER(config) = 7;
            PC_LINKP_FLAGS(config) = 0;
        }
        PC_LINKP_TRACK(config) = gTrackNum;
    }
}
