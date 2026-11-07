/* port/include/pc_online.h: the F1 Online page's pieces that need neither SDL nor the game (port/config/online.c;
 * docs/port/NETPLAY.md 13): the address a player types and the port, the recent hosts (net.recent), and the command
 * line of the restart into a session or a replay, quoted for Windows' CreateProcess and for the log. The page itself
 * is port/input/menu.c's; main's side (this PC's addresses, the replays, the restart) port/app/netmenu.c. */
#ifndef PC_ONLINE_H
#define PC_ONLINE_H

#include <stddef.h>

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PC_ONLINE_RECENT 8     /* net.recent keeps this many */
#define PC_ONLINE_ADDR 128     /* an address as typed, HOST[:PORT] */
#define PC_ONLINE_CODE 64      /* a join code (net.code's size) */

/* An address typed to join: the spaces around it trimmed, then a name or an IPv4 or IPv6 address, with :PORT after it
 * (an IPv6 address in brackets then). 0 with the trimmed text in out; -1 with a sentence for the player in why ("An
 * address has no spaces in it: 'my pc'"). port/net's own parse and the name's lookup still come in the session. */
int pc_online_address(const char *typed, char *out, size_t size, char *why, size_t why_size);
/* A port typed to host on: 1-65535. 0, or -1 with why. */
int pc_online_port(const char *typed, int *port, char *why, size_t why_size);

/* net.recent, the hosts joined last (comma-separated, the last first): into out, at most max; the count. */
int pc_online_recent(const char *list, char out[][PC_ONLINE_ADDR], int max);
/* The list with host put first: an address there already (letters' case aside, and a port of default_port being the
 * same as none) is taken out of its old place; at most PC_ONLINE_RECENT kept. 0, or -1 (list unchanged) when host
 * isn't an address or the list doesn't fit size. */
int pc_online_recent_add(char *list, size_t size, const char *host, int default_port);

/* The restart: this run's command line without what the restart replaces, then the restart's own options. */
enum { PC_RELAUNCH_OFFLINE, PC_RELAUNCH_HOST, PC_RELAUNCH_JOIN, PC_RELAUNCH_REPLAY };
typedef struct {
    int what;                  /* PC_RELAUNCH_* */
    const char *a, *b;         /* HOST: the port and the join code; JOIN: the address and the code; REPLAY: the file */
    const char *rom;           /* the ROM this run loaded: --rom when the command line names none (NULL, "": not) */
    int back;                  /* HOST, JOIN, REPLAY: --net-return (started from the page: its end comes back here) */
    const PcSettingValue *set; /* the overlay's changes not saved, as --set SECTION.KEY=VALUE each */
    int nset;
} PcRelaunch;
/* out[0] = argv[0]; then argv's options but a session's (--net-host [PORT], --net-join, --net-code, --net-return), a
 * replay's and a record's (--replay-inputs, --replay-force, --record-inputs, --input-delay, --hash-report) and the
 * press scripts' (--press, --press-file, --press-via), each with its value; then --rom, the --set changes, and the
 * restart's own: --net-host PORT --net-code CODE, --net-join ADDRESS --net-code CODE, or --replay-inputs FILE (with
 * --paced unless argv has --fast or --paced), and --net-return. out points into argv and into buf. The count, or -1
 * when out (max, with room for its NULL) or buf is too small. */
int pc_online_args(int argc, char *const *argv, const PcRelaunch *r, const char **out, int max, char *buf,
                   size_t bufsize);
/* One command line for CreateProcess: argv[0] in quotes, each argument as the C runtime's parser takes it back (in
 * quotes when it has a space, a tab or a quote, or is empty; a backslash doubled before a quote and at the end, a
 * quote after a backslash). The length, or -1 when it doesn't fit size. */
int pc_online_quote_windows(const char *const *argv, int argc, char *out, size_t size);
/* The same for the log, as a POSIX shell takes it ('...' where needed). */
int pc_online_quote_sh(const char *const *argv, int argc, char *out, size_t size);

#ifdef __cplusplus
}
#endif

#endif
