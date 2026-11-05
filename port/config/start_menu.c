/* port/config/start_menu.c: game.pc_setup, the main menu's PC SETUP option (pc_start_menu.h, docs/port/FRONTEND.md
 * "PC SETUP").
 *
 * The setting comes from main at start and from the settings overlay when it changes; the game thread reads it in
 * the main menu each frame, so it applies at once: the button comes or goes at the menu's next frame. Choosing PC
 * SETUP sets a request the main thread takes at its next tick (pc_menu_tick), which opens the overlay; the game is
 * then held there, on the menu, with PC SETUP still selected, until the overlay closes. */
#include <stdio.h>

#include "pc_start_menu.h"

static volatile int s_on;
static int s_open;   /* PC SETUP chosen, the overlay not yet opened: the game thread sets it, the main thread takes it */

void pc_start_menu_set(int on)
{
    s_on = on != 0;
}

int pc_start_menu_setting(void)
{
    return s_on;
}

int pc_start_valid(int index, int value)
{
    return index == PC_START_SLOT && s_on ? 1 : value;
}

const char *pc_start_label(int index, const char *label)
{
    return index == PC_START_SLOT && s_on ? "PC SETUP" : label;
}

int pc_start_exit(int exit, int select)
{
    if (exit != 1 || select != PC_START_SLOT || !s_on)
        return exit;
    __atomic_store_n(&s_open, 1, __ATOMIC_RELEASE);
    fprintf(stderr, "rush2049pc: PC SETUP chosen in the main menu: the settings open\n");
    return 0;
}

int pc_start_menu_take_open(void)
{
    return __atomic_exchange_n(&s_open, 0, __ATOMIC_ACQ_REL);
}
