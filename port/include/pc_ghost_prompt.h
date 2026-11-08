/* port/include/pc_ghost_prompt.h: game.ghost_prompt, the save prompt after a ghost race for every profile
 * (port/config/ghost_prompt.c, docs/port/SAVES.md "The ghost save prompt"). */
#ifndef PC_GHOST_PROMPT_H
#define PC_GHOST_PROMPT_H

void pc_ghost_prompt_set(int on);       /* main at start, the overlay when the setting changes */
int pc_ghost_prompt_setting(void);
int pc_ghost_prompt(int developer);     /* the game's: the race end's test of gSaveGhostFiles */

#endif
