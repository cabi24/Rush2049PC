/* port/app/netmenu.h: the F1 Online page's main side (port/app/netmenu.c, docs/port/NETPLAY.md 13), this PC's
 * addresses as the host's waiting screen and the page list them (netaddr.h), and an online session's input log
 * replayed offline as the session ran (netmenu.c).
 *
 * The page (port/input/menu.c) hosts, joins and watches a replay by restarting the program: lockstep starts at boot.
 * pc_net_menu_install gives it main's side; a choice there asks for a restart (pc_net_menu_restart_asked), the overlay
 * stops the main loop as its Quit does, and main's end (pc_net_menu_restart) runs the program again: exec on Linux,
 * CreateProcess on Windows. */
#ifndef PC_NETMENU_H
#define PC_NETMENU_H

#include <stddef.h>
#include <stdint.h>

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- netmenu.c: the page's side */

/* What this run is, for the page: offline, an online session, a replay. */
enum { PC_NET_MENU_OFFLINE, PC_NET_MENU_SESSION, PC_NET_MENU_REPLAY };
/* At start: this run's command line (the restart's base), and --net-return (a run the page started). */
void pc_net_menu_args(int argc, char **argv, int net_return);
/* With the overlay set up (after pc_input_init): the page's side (pc_menu_set_online_ui), what this run is, the data
 * folder (netplay/, replays/), the settings in effect (fullscreen as the file has it), the ROM loaded (a restart names
 * it), and this run's build and ROM SHA-1 as an input log's header says them (a log of another can't be watched). */
void pc_net_menu_install(int mode, const char *data_dir, const PcSettings *cfg, const char *rom, const char *build,
                         const char *rom_sha1);
/* A guest's join got the host's answer: its address first in net.recent, in the settings file read (cfg->source; not
 * with --no-config). */
void pc_net_menu_remember(PcSettings *cfg, const char *address);
/* The end of a run the page started (--net-return): the session ended or didn't start. started: the game had started
 * (pc_net_start). Unless the player stopped the program (the window closed, the quit key or Quit, Ctrl-C; in the game,
 * any end of this side's own), the restart without the session is asked for. */
void pc_net_menu_session_end(int started);
int pc_net_menu_restart_asked(void);
/* Before the log closes: says the restart's command line. Then pc_app_finish; then pc_net_menu_restart: the program
 * again (Linux: exec, returns only when it fails; Windows: CreateProcess, then waits for it and returns its exit code
 * when this run had a console or its output went somewhere, else returns 0 at once). code when no restart was asked. */
void pc_net_menu_restart_say(void);
int pc_net_menu_restart(int code);

/* --replay-inputs of an online session's input log (its header says session.rules=on): this run takes the session's
 * settings the game reads (pc_must_match's adopted ones, game.pak-pak4) for the run only, each change logged, and
 * reads the session's paks, the .p1.mpk and .p2.mpk beside the log, each checked against the header's hash. 0 when the
 * file isn't a session's log (or can't be read: the replay says why later), 1 when it is and all is taken, -1 with why
 * (a pak missing or not the session's). */
int pc_replay_session(const char *file, PcSettings *cfg, char *why, size_t why_size);
/* With pc_si_init(NULL, ...) done: the session's paks in the SI, in memory only (the replay writes nothing back). */
void pc_replay_session_paks(void);

#ifdef __cplusplus
}
#endif

#endif
