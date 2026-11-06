/* port/app/netplay.h: an online session, the game attached to port/net (docs/port/NETPLAY.md "Status: the session",
 * docs/port/NETWORK.md 10). main.c's side of it; port/app/netplay.c has the session itself.
 *
 * The flow, in main's boot order:
 *   1. pc_net_connect, after the ROM is loaded and the mods are read (mods_prepare), before the game's memory is
 *      mapped: this PC's settings (the build, the ROM's SHA-1, the mods, then the settings the game reads) and its
 *      controller 1's pak image go to the other PC, and the other's come back.
 *   2. The guest checks the host's identity lines (pc_net_blob_value), adopts the host's settings for the session
 *      (pc_net_adopt: never written to its settings file), or refuses with a clear reason (pc_net_reject).
 *   3. pc_net_paks: the two images checked; main goes on with its setup (memory, the game's data...).
 *   4. pc_net_install_paks: port 1 holds the host's image, port 2 the guest's, in memory only; ports 3 and 4 none.
 *   5. pc_net_begin with the must-match text (main's, computed now): the pads source, the state hash, the input log.
 *   6. pc_net_start: both sides ready; the start time; then main boots the game.
 *   7. pc_net_finish after the main loop: BYE, the end's reason shown, the session's numbers in the log. */
#ifndef PC_NETPLAY_H
#define PC_NETPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "pc_lockstep.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { PC_NET_NONE = 0, PC_NET_HOST = 1, PC_NET_GUEST = 2 };

/* The command line's (the [net] settings give the port, the delay and the join code). */
typedef struct {
    int role;                /* PC_NET_*: --net-host, --net-join */
    int port;                /* --net-host PORT: the port to listen on (0: any, said in the log); -1: net.port */
    const char *join;        /* --net-join HOST[:PORT] */
    const char *bind;        /* --net-bind ADDR: the local address ("" or NULL: any, both IPv4 and IPv6) */
    int timeout_ms;          /* --net-timeout: silence that ends a session (0: 15 s) */
    int paced;               /* the paced clock (a session without --fast): RUSH_PACE_SPEED sets the retrace's length */
} PcNetOptions;

int pc_net_role(void);   /* PC_NET_* once pc_net_connect has run; PC_NET_NONE without a session */

/* 1. Connects (the host waits for a guest, up to 10 minutes; the guest for an answer, up to the timeout), sends blob
 * (main's settings text: "key=value" lines) and this PC's controller 1's pak (NULL: none), and waits until the other
 * PC's have arrived. build is what both must run (the library compares it first). 0, or the program's exit code
 * with the reason logged and shown (window: a message box). */
int pc_net_connect(const PcNetOptions *o, const PcSettings *cfg, const char *build, const char *blob,
                   const uint8_t *pak, int window);
/* The guest: the host's settings text (after pc_net_connect). */
const char *pc_net_host_settings(void);
/* The value of a "key=value" line of such a text, into out: 0, or -1 when it has none. */
int pc_net_blob_value(const char *blob, const char *key, char *out, size_t size);
/* 2. The guest takes the host's settings the game reads (pc_must_match's adopted ones) into cfg, for this run only,
 * each change logged ("game.cars 8 -> original (the host's)"). *mods_inputs: game.memory, expansion_pak or cars
 * changed, which the mods' memory plan depends on. 0, or -1 with why (a setting missing or not usable here). */
int pc_net_adopt(PcSettings *cfg, const char *host, int *mods_inputs, char *why, size_t why_size);
/* Ends the session refused (NET_END_REJECTED), shows why; for_peer: the reason the other PC gets (63 characters at
 * most; NULL: why). The exit code. */
int pc_net_reject(const char *why, const char *for_peer, int window);
/* 3. The two paks: the host's (port 1) and the guest's (port 2), each checked (pc_pak_image_check), or none. A
 * damaged one ends the session. 0, or the exit code. */
int pc_net_paks(int window);
/* 4. Installs them in the SI (pc_si_set_pak_image) and sets cfg's accessories to what the session has (game.pak and
 * pak2 memory or none, pak3 and pak4 none), which the must-match text then says. */
void pc_net_install_paks(PcSettings *cfg);
/* 5. With main's must-match text (docs/port/LOCKSTEP.md 3, taken with the session's paks in): the pads source over
 * the session, the state hash on (every retrace; the window every 60 unless --log-hash said), the last 240
 * retraces' curated state kept for a desync's report, the session's rules (no hold, the overlay's locks), and the
 * input log in the data folder's netplay/ (ls->record, with the must-match text as its header). quit_key names the
 * key that leaves ("" for none). 0, or the exit code. */
int pc_net_begin(PcLockstepOptions *ls, const char *must_match, const char *data_dir, const char *quit_key,
                 int window);
/* 6. Both PCs ready: the start time agreed; waits for it. 0, or the exit code. */
int pc_net_start(int window);
/* 7. After the main loop: this side's BYE when it left (after the other PC has its last inputs), the reason the
 * session ended shown (a message box when it wasn't this player's choice), the session's numbers and its input log
 * in the log. The exit code: code, or 3 when the session ended otherwise than a player leaving. */
int pc_net_finish(int code, int window);

#ifdef __cplusplus
}
#endif

#endif
