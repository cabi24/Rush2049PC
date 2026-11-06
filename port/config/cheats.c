/* port/config/cheats.c: game.cheats_menu and game.cheats_unlock (pc_cheats.h, docs/port/FRONTEND.md "The cheat
 * screen in the main menu").
 *
 * game.cheats_menu: the main menu's CHEATS option (slot 6) is valid while it is on, as when its code was entered;
 * optionValid's existing PC hook (PC_START_VALID, pc_start_valid in start_menu.c) asks pc_cheats_menu_setting. Only
 * the option: the developer name's other unlocks (the ghost save prompt, cheat states 14-16) don't change.
 * game.cheats_unlock: the cheat screen's tests of a cheat's flag (PC_CHEAT_ACTIVE in cheatscr.c: may it be switched,
 * how it is drawn) take every cheat as available, its code not needed. The flags themselves are not written: a code
 * entered still sets its cheat's, as on the N64, and with the setting off again each cheat is as its flag says. A
 * cheat switched on stays on (its game flag) as the cartridge keeps it, until switched off or the game restarts.
 *
 * The settings come from main at start and from the settings overlay when they change; the game thread reads them in
 * the main menu and on the cheat screen each frame, so they apply at once. */
#include "pc_cheats.h"

static volatile int s_menu, s_unlock;

void pc_cheats_menu_set(int on)
{
    s_menu = on != 0;
}

int pc_cheats_menu_setting(void)
{
    return s_menu;
}

void pc_cheats_unlock_set(int on)
{
    s_unlock = on != 0;
}

int pc_cheats_unlock_setting(void)
{
    return s_unlock;
}

int pc_cheat_active(int active)
{
    return active || s_unlock;
}
