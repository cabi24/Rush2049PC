/* port/app/netkeep.c: the data folder's netplay/ kept to the newest online sessions (net.keep; docs/port/NETPLAY.md
 * 17). No SDL, no game: the tests link it alone (port/app/tests/test_netkeep.c).
 *
 * Each session leaves DATE-ROLE.pads (its input log), DATE-ROLE.p1.mpk and .p2.mpk (its paks) and, after a desync,
 * DATE-ROLE.desync.txt in netplay/ (netplay.c's name_files: DATE is YYYY-MM-DD_HHMMSS, ROLE host or guest, with -N
 * after it when that name was taken). When a session starts, the sessions there beyond the newest `keep` (the one
 * starting counted) are removed, the newest by their names (which start with their date and time, as the F1 Online
 * page's list sorts them). Never removed:
 *   - the session starting (its base given);
 *   - a session any of whose files changed in the last PC_NET_KEEP_QUIET_S seconds: one another copy of the program
 *     on this PC may still be playing, or waiting up to net.rejoin's hour for its other player to come back;
 *   - anything else in netplay/: the rejoin tickets (rejoin-host.txt, rejoin-guest.txt), files not named as a session's
 *     are, other folders; and replays/, where F1's Keep this replay copies a session, isn't looked at. */
#define _GNU_SOURCE
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "app.h"
#include "netkeep.h"

static const char *const suffixes[] = {".pads", ".p1.mpk", ".p2.mpk", ".desync.txt"};
#define NSUFFIX (int)(sizeof suffixes / sizeof *suffixes)

/* name's session base (DATE-ROLE[-N]) into base when name is one of a session's files: its length, else 0. */
static size_t session_base(const char *name, char *base, size_t size)
{
    size_t n = strlen(name), b;
    int k;
    for (k = 0; k < NSUFFIX; k++) {
        size_t s = strlen(suffixes[k]);
        if (n > s && !strcmp(name + n - s, suffixes[k]))
            break;
    }
    if (k == NSUFFIX)
        return 0;
    b = n - strlen(suffixes[k]);
    /* YYYY-MM-DD_HHMMSS */
    static const char form[] = "dddd-dd-dd_dddddd";
    if (b < sizeof form - 1 + 5)
        return 0;
    for (size_t i = 0; i < sizeof form - 1; i++)
        if (form[i] == 'd' ? name[i] < '0' || name[i] > '9' : name[i] != form[i])
            return 0;
    const char *role = name + sizeof form - 1, *end = name + b;
    size_t r = !strncmp(role, "-host", 5) ? 5 : !strncmp(role, "-guest", 6) ? 6 : 0;
    if (!r)
        return 0;
    if (role + r != end) {   /* -N */
        const char *d = role + r;
        if (*d++ != '-' || d == end)
            return 0;
        for (; d < end; d++)
            if (*d < '0' || *d > '9')
                return 0;
    }
    if (b >= size)
        return 0;
    memcpy(base, name, b);
    base[b] = 0;
    return b;
}

typedef struct {
    char base[96];
    long long newest;   /* its files' latest change, seconds */
} Session;

static int newest_first(const void *a, const void *b)
{
    return strcmp(((const Session *)b)->base, ((const Session *)a)->base);
}

int pc_net_keep(const char *dir, const char *current, int keep, long long now, int *kept, char *say, size_t say_size)
{
    DIR *d;
    struct dirent *e;
    Session *s = NULL;
    int n = 0, cap = 0, removed = 0, k, others = 0, room;
    char base[96], path[PC_APP_PATH + 128];
    if (kept)
        *kept = 0;
    if (say && say_size)
        say[0] = 0;
    if (keep <= 0 || !dir || !(d = opendir(dir)))
        return 0;   /* all kept, or no folder yet */
    while ((e = readdir(d)) != NULL) {
        struct stat st;
        if (!session_base(e->d_name, base, sizeof base))
            continue;
        pc_app_join(path, sizeof path, dir, e->d_name);
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        for (k = 0; k < n && strcmp(s[k].base, base); k++)
            ;
        if (k == n) {
            if (n == cap) {
                Session *grown = realloc(s, (size_t)(cap = cap ? 2 * cap : 64) * sizeof *s);
                if (!grown)
                    break;
                s = grown;
            }
            snprintf(s[n].base, sizeof s[n].base, "%s", base);
            s[n++].newest = 0;
        }
        if ((long long)st.st_mtime > s[k].newest)
            s[k].newest = (long long)st.st_mtime;
    }
    closedir(d);
    if (n > 1)   /* none found leaves s null, which qsort may not be given (C 7.22.5) */
        qsort(s, (size_t)n, sizeof *s, newest_first);
    room = current && *current ? keep - 1 : keep;   /* the session starting is one of the newest */
    for (int i = 0; i < n; i++) {
        if (current && !strcmp(s[i].base, current))
            continue;
        if (++others <= room || now - s[i].newest < PC_NET_KEEP_QUIET_S) {
            if (kept)
                (*kept)++;
            continue;   /* among the newest, or perhaps still played */
        }
        for (k = 0; k < NSUFFIX; k++) {
            snprintf(base, sizeof base, "%s%s", s[i].base, suffixes[k]);
            pc_app_join(path, sizeof path, dir, base);
            if (pc_app_exists(path) && remove(path) != 0)
                fprintf(stderr, "rush2049pc: online: netplay/: can't remove %s\n", path);
        }
        if (say && say_size && removed < 3) {   /* the first few, by name */
            size_t used = strlen(say);
            snprintf(say + used, say_size - used, "%s%s", removed ? ", " : "", s[i].base);
        }
        removed++;
    }
    free(s);
    return removed;
}
