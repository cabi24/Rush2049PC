/* port/app/tests/test_netkeep.c: netplay/ kept to the newest online sessions (port/app/netkeep.c, net.keep;
 * docs/port/NETPLAY.md 17), in a temporary folder under the directory given: the newest kept, the session starting
 * kept and counted, the older ones' four kinds of file removed and nothing else, a session played lately kept however
 * old its name, the rejoin tickets, other files and folders untouched, keep = all and a missing folder. */
#define _GNU_SOURCE
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "app.h"
#include "netkeep.h"

static int failures, checks;

#define CHECK(cond, ...)                                                                                          \
    do {                                                                                                          \
        checks++;                                                                                                 \
        if (!(cond)) {                                                                                            \
            failures++;                                                                                           \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                           \
            printf(__VA_ARGS__);                                                                                  \
            printf("\n");                                                                                         \
        }                                                                                                         \
    } while (0)

static char dir[PC_APP_PATH];
static long long now;

/* A file of dir, written, its time `age` seconds ago. */
static void put(const char *name, long long age)
{
    char p[PC_APP_PATH + 64];
    struct utimbuf t;
    pc_app_join(p, sizeof p, dir, name);
    FILE *f = fopen(p, "wb");
    if (!f || fputs(name, f) < 0 || fclose(f) != 0) {
        printf("cannot write %s\n", p);
        exit(1);
    }
    t.actime = t.modtime = (time_t)(now - age);
    utime(p, &t);
}

static int there(const char *name)
{
    char p[PC_APP_PATH + 64];
    pc_app_join(p, sizeof p, dir, name);
    return pc_app_exists(p);
}

/* Session i's base: older for a larger i (its date a day earlier for each). */
static void base_of(int i, const char *role, char *out, size_t n)
{
    time_t t = (time_t)(now - (long long)i * 86400);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%Y-%m-%d_%H%M%S", gmtime(&t));
    snprintf(out, n, "%s-%s", stamp, role);
}

static void session(int i, const char *role, long long age, int paks, int report)
{
    char b[96], name[128];
    base_of(i, role, b, sizeof b);
    snprintf(name, sizeof name, "%s.pads", b), put(name, age);
    if (paks) {
        snprintf(name, sizeof name, "%s.p1.mpk", b), put(name, age);
        snprintf(name, sizeof name, "%s.p2.mpk", b), put(name, age);
    }
    if (report)
        snprintf(name, sizeof name, "%s.desync.txt", b), put(name, age);
}

static int session_there(int i, const char *role)
{
    char b[96], name[128];
    base_of(i, role, b, sizeof b);
    snprintf(name, sizeof name, "%s.pads", b);
    return there(name);
}

int main(int argc, char **argv)
{
    char top[PC_APP_PATH], say[200], name[160], cur[96];
    int kept, gone;
    now = (long long)time(NULL);
    snprintf(top, sizeof top, "%s", argc > 1 ? argv[1] : "test-scratch-netkeep");
    pc_app_join(dir, sizeof dir, top, "netplay");
    {   /* the last run's files out (a level of folders under it, empty) */
        DIR *d = opendir(dir);
        struct dirent *e;
        char p[PC_APP_PATH + 300];
        while (d && (e = readdir(d)) != NULL)
            if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
                pc_app_join(p, sizeof p, dir, e->d_name);
                if (remove(p) != 0)
                    rmdir(p);
            }
        if (d)
            closedir(d);
        rmdir(dir);
    }
    CHECK(pc_net_keep(dir, NULL, 20, now, &kept, say, sizeof say) == 0 && kept == 0, "no folder: nothing");
    CHECK(pc_app_mkdirs(dir) == 0, "the folder made");
    /* no session in it (a first session's start): nothing to sort, and nothing given qsort (UBSan's nonnull, CI) */
    CHECK(pc_net_keep(dir, NULL, 20, now, &kept, say, sizeof say) == 0 && kept == 0, "an empty folder: nothing");

    /* 25 old sessions (a day to 25 days old by their names, written as long ago), every second one a guest's with its
     * paks, a few with a desync report; one -2 of a second in the same second; the session starting (0) */
    for (int i = 1; i <= 25; i++)
        session(i, i % 2 ? "host" : "guest", (long long)i * 86400, i % 2 == 0, i % 7 == 0);
    base_of(3, "host-2", name, sizeof name);   /* the same second as session 3's, taken: -2 (newer by its name) */
    strcat(name, ".pads");
    put(name, 3 * 86400);
    base_of(0, "host", cur, sizeof cur);
    snprintf(name, sizeof name, "%s.p1.mpk", cur), put(name, 0);   /* its log isn't written yet: its pak is */
    /* not a session's: the tickets, a file of another name, one with a session's suffix but not its name, a folder */
    put("rejoin-host.txt", 30 * 86400);
    put("rejoin-guest.txt", 30 * 86400);
    put("notes.txt", 30 * 86400);
    put("2026-11-07_2104-host.pads", 30 * 86400);
    put("2020-01-01_000000-friend.pads", 30 * 86400);
    put("2020-01-01_000000-host.pads.bak", 30 * 86400);
    {
        char sub[PC_APP_PATH + 64];
        pc_app_join(sub, sizeof sub, dir, "2020-01-01_000000-host.p1.mpk.d");
        pc_app_mkdirs(sub);
    }

    /* keep 20: the session starting and the 19 newest others; 26 others, so 7 go */
    gone = pc_net_keep(dir, cur, 20, now, &kept, say, sizeof say);
    CHECK(gone == 7 && kept == 19, "keep 20: %d removed, %d kept besides the one starting", gone, kept);
    {
        char b3[96], two[128];
        base_of(3, "host", b3, sizeof b3);
        snprintf(two, sizeof two, "%s-2.pads", b3);
        CHECK(there(two) && session_there(3, "host"), "the -2 and its first both kept (newest)");
        base_of(19, "host", b3, sizeof b3);
        CHECK(!strncmp(say, b3, strlen(b3)), "said by name, the newest of them first: '%s'", say);
    }
    for (int i = 1; i <= 18; i++)
        CHECK(session_there(i, i % 2 ? "host" : "guest"), "session %d of the newest kept", i);
    for (int i = 19; i <= 25; i++) {
        char b[96], n2[128];
        CHECK(!session_there(i, i % 2 ? "host" : "guest"), "session %d removed", i);
        base_of(i, i % 2 ? "host" : "guest", b, sizeof b);
        snprintf(n2, sizeof n2, "%s.p1.mpk", b);
        CHECK(!there(n2), "session %d's pak removed", i);
        snprintf(n2, sizeof n2, "%s.desync.txt", b);
        CHECK(!there(n2), "session %d's report removed", i);
    }
    snprintf(name, sizeof name, "%s.p1.mpk", cur);
    CHECK(there(name), "the session starting kept");
    CHECK(there("rejoin-host.txt") && there("rejoin-guest.txt"), "the rejoin tickets untouched");
    CHECK(there("notes.txt") && there("2026-11-07_2104-host.pads") && there("2020-01-01_000000-friend.pads") &&
              there("2020-01-01_000000-host.pads.bak") && there("2020-01-01_000000-host.p1.mpk.d"),
          "files and folders not named as a session's are untouched");

    /* a session played lately is kept however old its name (another copy of the program may play it) */
    session(40, "guest", 60, 1, 0);       /* a minute ago */
    session(41, "host", PC_NET_KEEP_QUIET_S - 60, 0, 0);
    session(42, "host", PC_NET_KEEP_QUIET_S + 60, 0, 0);
    gone = pc_net_keep(dir, cur, 20, now, &kept, say, sizeof say);
    CHECK(gone == 1 && session_there(40, "guest") && session_there(41, "host") && !session_there(42, "host"),
          "played in the last two hours: kept (%d removed: %s)", gone, say);

    /* a session's file changed lately keeps the whole session */
    {
        char b[96];
        base_of(18, "guest", b, sizeof b);
        snprintf(name, sizeof name, "%s.desync.txt", b), put(name, 10);
    }
    gone = pc_net_keep(dir, cur, 2, now, &kept, say, sizeof say);
    CHECK(session_there(18, "guest") && session_there(40, "guest") && session_there(41, "host"),
          "keep 2: the ones played lately stay");
    CHECK(session_there(1, "host") && !session_there(2, "guest") && !session_there(17, "host"),
          "keep 2: the newest other kept, the rest not played lately removed");
    CHECK(kept == 4, "keep 2: the newest other and the three played lately: %d kept, %d removed", kept, gone);

    /* keep all, and keep 1 with no session starting */
    session(50, "host", 50 * 86400, 1, 1);
    CHECK(pc_net_keep(dir, cur, 0, now, &kept, say, sizeof say) == 0 && session_there(50, "host"), "all: nothing");
    gone = pc_net_keep(dir, NULL, 1, now, &kept, say, sizeof say);
    CHECK(!session_there(50, "host") && kept == 4, "keep 1, none starting: the newest and the recent ones (%d kept)",
          kept);
    snprintf(name, sizeof name, "%s.p1.mpk", cur);
    CHECK(there(name), "a session of its pak alone is one (the newest, 0 s old)");

    printf("netkeep: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
