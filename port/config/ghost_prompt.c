/* port/config/ghost_prompt.c: game.ghost_prompt, the save prompt after a ghost race for every profile
 * (docs/port/SAVES.md "The ghost save prompt").
 *
 * After a ghost race whose car finished its laps, the cartridge offers to save the ghost to a controller pak (high
 * score state 4: pick a pak, save or not) only when the profile in use has the developer's name: LoadPlayerData sets
 * D_801148E4 for that name alone, with the cheat screen. Any other profile's ghost stays in memory for the session.
 * With the setting on, billboard_render's test of the flag takes it as set (PC_GHOST_PROMPT), so every profile gets
 * the prompt; nothing else the name unlocks changes. Off (the default) it gives the flag as it is. The setting comes
 * from main at start and from the settings overlay, and is read at each race's end. */
#include "pc_ghost_prompt.h"

static volatile int s_on;

void pc_ghost_prompt_set(int on)
{
    s_on = on != 0;
}

int pc_ghost_prompt_setting(void)
{
    return s_on;
}

int pc_ghost_prompt(int developer)
{
    return developer || s_on;
}
