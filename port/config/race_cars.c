/* port/config/race_cars.c: the cars a race holds, [game] cars (docs/port/FRONTEND.md, docs/port/MORECARS.md 10.1).
 *
 * The setting comes from main at start (rush2049pc.ini, --set game.cars) and from the settings overlay when it changes
 * (pc_race_cars_set): 0 for original, the N64's 6, or 6 to PC_MAX_LINKS. The game reads the count through
 * pc_race_cars() (PC_RACE_CARS, pc_game_types.h) all over a race, on its game and model threads, so that is a copy it
 * latches where a race's roster is filled (pc_race_cars_begin): a change in the overlay, which holds the game while it
 * is open, applies from the next race and never in the middle of one. Until the first pc_race_cars_begin it is the
 * N64's 6. */
#include "config.h"
#include "pc_game_types.h"

static volatile int s_setting = PC_N64_CARS;   /* the setting, 6 to PC_MAX_LINKS */
static volatile int s_race = PC_N64_CARS;      /* what the last pc_race_cars_begin latched */

void pc_race_cars_set(int cars)
{
    s_setting = cars < PC_N64_CARS ? PC_N64_CARS : cars > PC_MAX_LINKS ? PC_MAX_LINKS : cars;
}

int pc_cars_setting(void)
{
    return s_setting;
}

void pc_race_cars_begin(void)
{
    s_race = s_setting;
}

int pc_race_cars(void)
{
    return s_race;
}
