/* CalcPolePositions: the race's roster and starting places (MAT3 game.c). */
#include "game/game.h"

/* A circuit's car (tCircuitCar, gCircuitCars): its id, the place it starts from. */
typedef struct Player {
    /* 0 */ u8 pad0;
    /* 1 */ u8 id;
    /* 2 */ u8 pad2[74];
} Player; /* 0x4C */

VIEW(Player[], gCircuitCars);
#define gCircuitCars gCircuitCars_view
/* The race roster is the PC's pc_links (pc_game_types.h, docs/port/DECOUPLE.md); the number of cars a race holds
 * is the PC's setting (PC_RACE_CARS: 6 by default), a circuit always the N64's 6 (docs/port/MORECARS.md). */
#define RACE_CARS (gRace == kCircuit ? PC_N64_CARS : PC_RACE_CARS)

/* Fills the roster (each link: its place on the grid, who drives it, its flags and the track): the players
 * (driven here, owner 6) start behind the drones (owner 0), as many drones as asked for and fit (none in stunt,
 * obstacle and battle). In a circuit after its first race the cars keep their circuit places (the two players' own
 * first), else a circuit's drones take random places. Links past the race's cars are empty (owner 7). */
void CalcPolePositions(void)
{
    s32 extra;
    s32 slot;
    s32 prior;
    u8 next = 0;
    PC_LINK *config;

    if (!(gGameState & GS_HIGH_SCORE)) pc_race_cars_begin(); extra = gNumDrones;
    if (RACE_CARS - gNumPlayers < extra) {
        extra = RACE_CARS - gNumPlayers;
    }
    if (gRace == kStunt || gRace == kObstacle || gRace == kBattle) {
        extra = 0;
    }
    gNumCars = gNumPlayers + extra;
    for (slot = 0, config = PC_LINKS; slot < PC_MAX_LINKS; slot++, config++) {
        if (gRace == kCircuit && slot < gNumCars && gCircuitTrack > 0) {
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
            if (gRace == kCircuit) {
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
