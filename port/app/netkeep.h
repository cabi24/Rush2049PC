/* port/app/netkeep.h: the data folder's netplay/ kept to the newest online sessions (net.keep; docs/port/NETPLAY.md
 * 17). No SDL, no game. */
#ifndef PC_NETKEEP_H
#define PC_NETKEEP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A session whose files changed this recently is never removed: another copy of the program may still play it, or wait
 * for its other player (net.rejoin, at most an hour). */
#define PC_NET_KEEP_QUIET_S (2 * 60 * 60)

/* The sessions in dir (netplay/) beyond the newest `keep`, removed: each one's .pads, .p1.mpk, .p2.mpk and
 * .desync.txt. current: the base (DATE-ROLE[-N]) of the session starting, never removed and counted as one of keep
 * (NULL or "": none). now: the time, seconds since 1970. keep <= 0: none removed. The count removed; *kept the
 * sessions left besides current; say: the first few removed, by name. */
int pc_net_keep(const char *dir, const char *current, int keep, long long now, int *kept, char *say, size_t say_size);

#ifdef __cplusplus
}
#endif

#endif
