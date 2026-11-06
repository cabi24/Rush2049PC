/* port/include/pc_cheats.h: game.cheats_menu and game.cheats_unlock, the cheat screen in the main menu and every
 * cheat on it available (port/config/cheats.c, docs/port/FRONTEND.md "The cheat screen in the main menu").
 *
 * The main menu's option slot 6 is the cheat screen (state 0x8000000), valid on the N64 only once gCheatActive[21]
 * (D_801164A1) is set: by the main menu's own button code (CheckForEgg, func_8038F648) or by LoadPlayerData for a
 * profile named as the cartridge's developer code (D_801211CC). On the cheat screen each of the 21 cheats is
 * available (switched by LEFT and RIGHT, drawn bright) only once gCheatActive[cheat] (D_8011648C[cheat]) is set, which
 * only its own button code does there, for the session: nothing saves it, and nothing else sets it. Off (the default)
 * each call gives the N64's value. */
#ifndef PC_CHEATS_H
#define PC_CHEATS_H

#define PC_CHEATS_SLOT 6                /* the main menu's option slot of the cheat screen */

void pc_cheats_menu_set(int on);        /* main at start, the overlay when the setting changes */
int pc_cheats_menu_setting(void);
void pc_cheats_unlock_set(int on);
int pc_cheats_unlock_setting(void);
/* The game's (the game thread): the cheat screen's tests of gCheatActive[cheat] (PC_CHEAT_ACTIVE, cheatscr.c): 1 for
 * every cheat while game.cheats_unlock is on, else the flag as it is. The main menu's slot 6 goes through
 * pc_start_valid (start_menu.c). */
int pc_cheat_active(int active);

#endif
