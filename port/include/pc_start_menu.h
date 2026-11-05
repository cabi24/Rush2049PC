/* port/include/pc_start_menu.h: game.pc_setup, the main menu's PC SETUP option (docs/port/FRONTEND.md "PC SETUP").
 *
 * The main menu (updateStartScreen, src/ovl_a/groups/start_screen/group.c) has ten option slots, each valid or not
 * (optionValid): 0 the race, 1-4 its four screens, 6 the cheat screen once its code is entered, 7 and 9 for a
 * debug build, 5 and 8 never. With game.pc_setup on, slot 8 is valid: the menu gives it a button of its own below
 * the others, as it does the cheat screen, its label is PC SETUP, and choosing it opens the settings overlay over the
 * menu (port/input/menu.c) instead of leaving it. Off (the default) every call gives the N64's value. */
#ifndef PC_START_MENU_H
#define PC_START_MENU_H

#define PC_START_SLOT 8                 /* the option slot PC SETUP takes */

void pc_start_menu_set(int on);         /* main at start, the overlay when the setting changes */
int pc_start_menu_setting(void);
/* The game's (the game thread): optionValid's value for a slot; a slot's label; the menu's exit choice (1: A on the
 * selected slot) as the menu should take it: 0 when it was PC SETUP, whose settings then open */
int pc_start_valid(int index, int value);
const char *pc_start_label(int index, const char *label);
int pc_start_exit(int exit, int select);
/* The main thread's (menu.c's tick): 1 once after PC SETUP was chosen */
int pc_start_menu_take_open(void);

#endif
