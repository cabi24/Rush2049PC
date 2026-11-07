/* port/config/online.c: the F1 Online page's pieces without SDL or the game (port/include/pc_online.h; docs/port/
 * NETPLAY.md 13): the address and the port a player types, net.recent, and the restart's command line.
 *
 * The restart. Lockstep starts at boot (the handshake comes before the game's memory is mapped), so hosting, joining
 * or watching a replay from the page starts the program again with --net-host, --net-join or --replay-inputs and the
 * options this run had: port/app/netmenu.c then execs it (Linux) or starts it with CreateProcess (Windows). The options
 * a restart replaces are left out wherever they are. --net-return marks the page's own restart options: it and every
 * option after it belong to that restart, so the next restart (leaving the session, the session's end) leaves them out
 * and the run is the one before again. */
#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pc_online.h"

/* ---------------------------------------------------------------- the address and the port */

static int say(char *why, size_t n, const char *fmt, const char *arg)
{
    if (why && n)
        snprintf(why, n, fmt, arg);
    return -1;
}

/* A port's digits: 1-65535; -1 otherwise. */
static long port_of(const char *s, size_t n)
{
    long p = 0;
    if (n == 0 || n > 5)
        return -1;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9')
            return -1;
        p = p * 10 + (s[i] - '0');
    }
    return p >= 1 && p <= 65535 ? p : -1;
}

/* An IPv4 address's form: four numbers 0-255 (1 when it is one, 0 when it isn't all digits and dots, -1 when it is
 * but not four numbers 0-255). */
static int ipv4(const char *s, size_t n)
{
    int parts = 0, digits = 0;
    long v = 0;
    for (size_t i = 0; i < n; i++)
        if (!isdigit((unsigned char)s[i]) && s[i] != '.')
            return 0;
    for (size_t i = 0; i <= n; i++) {
        if (i == n || s[i] == '.') {
            if (!digits || v > 255)
                return -1;
            parts++, digits = 0, v = 0;
        } else if (++digits > 3) {
            return -1;
        } else {
            v = v * 10 + (s[i] - '0');
        }
    }
    return parts == 4 ? 1 : -1;
}

int pc_online_address(const char *typed, char *out, size_t size, char *why, size_t why_size)
{
    char s[PC_ONLINE_ADDR * 2];
    const char *host, *end, *colon;
    size_t n, hn;
    if (!typed)
        typed = "";
    while (*typed == ' ' || *typed == '\t' || *typed == '\r' || *typed == '\n')
        typed++;
    n = strlen(typed);
    while (n && strchr(" \t\r\n", typed[n - 1]))
        n--;
    if (n == 0)
        return say(why, why_size, "%sType the host's address first: their Tailscale name or 100.x address", "");
    if (n >= PC_ONLINE_ADDR || n >= sizeof s)
        return say(why, why_size, "%sThat is too long for an address", "");
    memcpy(s, typed, n);
    s[n] = 0;
    if (strstr(s, "://")) {
        char scheme[24];
        snprintf(scheme, sizeof scheme, "%.*s", (int)(strstr(s, "://") - s < 16 ? strstr(s, "://") - s : 16), s);
        return say(why, why_size, "Just the address, without '%s://': my-pc or 100.101.102.103", scheme);
    }
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        char one[2] = {s[i], 0};
        if (c == ' ' || c == '\t')
            return say(why, why_size, "An address has no spaces in it: '%s'", s);
        if (c == '/' || c == '\\')
            return say(why, why_size, "Just the address, without a path after it: '%s'", s);
        if (c == '@')
            return say(why, why_size, "Just the address, without a name and '@': '%s'", s);
        if (c < 0x21 || c > 0x7E)
            return say(why, why_size, "%sAn address has only letters, digits, dots, '-' and ':'", "");
        if (!isalnum(c) && !strchr(".-_:[]%", c))
            return say(why, why_size, "'%s' can't be in an address: a name (my-pc) or an address (100.101.102.103)",
                       one);
    }
    if (s[0] == '[') {   /* [IPv6]:PORT */
        end = strchr(s, ']');
        if (!end)
            return say(why, why_size, "An IPv6 address in brackets needs its ']': [%s]:24049", s + 1);
        host = s + 1, hn = (size_t)(end - host);
        if (hn == 0 || !memchr(host, ':', hn))
            return say(why, why_size, "In brackets goes an IPv6 address, with ':' in it: '%s'", s);
        if (end[1] && (end[1] != ':' || port_of(end + 2, strlen(end + 2)) < 0))
            return say(why, why_size, "After ']' comes ':' and the port, 1-65535: '%s'", s);
    } else {
        colon = strchr(s, ':');
        if (colon && strchr(colon + 1, ':')) {   /* two: a bare IPv6 address, no port */
            if (strchr(s, '[') || strchr(s, ']'))
                return say(why, why_size, "An IPv6 address with a port goes in brackets: [fd7a::1]:24049, not '%s'",
                           s);
            return snprintf(out, size, "%s", s) < (int)size ? 0 : say(why, why_size, "%sToo long", "");
        }
        if (strchr(s, '[') || strchr(s, ']'))
            return say(why, why_size, "Brackets are for an IPv6 address: [fd7a::1]:24049, not '%s'", s);
        hn = colon ? (size_t)(colon - s) : n;
        host = s;
        if (hn == 0)
            return say(why, why_size, "The name or address comes before ':': '%s'", s);
        if (colon && port_of(colon + 1, strlen(colon + 1)) < 0)
            return say(why, why_size, "The port after ':' is a number from 1 to 65535: '%s'", s);
        if (ipv4(host, hn) < 0)
            return say(why, why_size, "'%s' isn't an IPv4 address: four numbers 0-255 with dots between them", s);
        if (host[0] == '.' || host[0] == '-' || strstr(s, ".."))
            return say(why, why_size, "'%s' isn't a name: names are words with dots between them (my-pc.ts.net)", s);
    }
    if (snprintf(out, size, "%s", s) >= (int)size)
        return say(why, why_size, "%sThat is too long for an address", "");
    return 0;
}

int pc_online_port(const char *typed, int *port, char *why, size_t why_size)
{
    char s[32];
    size_t n;
    while (typed && (*typed == ' ' || *typed == '\t'))
        typed++;
    snprintf(s, sizeof s, "%s", typed ? typed : "");
    for (n = strlen(s); n && (s[n - 1] == ' ' || s[n - 1] == '\t'); n--)
        s[n - 1] = 0;
    long p = port_of(s, strlen(s));
    if (p < 0) {
        if (why && why_size)
            snprintf(why, why_size, s[0] ? "The port is a number from 1 to 65535, not '%s'"
                                         : "The port is a number from 1 to 65535 (24049 is the usual one)%s", s);
        return -1;
    }
    *port = (int)p;
    return 0;
}

/* ---------------------------------------------------------------- the recent hosts */

int pc_online_recent(const char *list, char out[][PC_ONLINE_ADDR], int max)
{
    int n = 0;
    const char *p = list ? list : "";
    while (*p && n < max) {
        const char *e = strchr(p, ',');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char item[PC_ONLINE_ADDR * 2];
        snprintf(item, sizeof item, "%.*s", (int)(len < sizeof item ? len : sizeof item - 1), p);
        if (pc_online_address(item, out[n], PC_ONLINE_ADDR, NULL, 0) == 0)
            n++;
        p = e ? e + 1 : p + len;
    }
    return n;
}

/* Two addresses are one host: the host part alike but for its letters' case, and the port alike (none being
 * default_port). */
static int same_host(const char *a, const char *b, int default_port)
{
    char ha[PC_ONLINE_ADDR], hb[PC_ONLINE_ADDR];
    int pa = default_port, pb = default_port;
    for (int k = 0; k < 2; k++) {
        const char *s = k ? b : a, *colon;
        char *h = k ? hb : ha;
        int *port = k ? &pb : &pa;
        if (s[0] == '[') {
            const char *end = strchr(s, ']');
            snprintf(h, PC_ONLINE_ADDR, "%.*s", end ? (int)(end - s - 1) : 0, s + 1);
            if (end && end[1] == ':')
                *port = atoi(end + 2);
            continue;
        }
        colon = strchr(s, ':');
        if (colon && !strchr(colon + 1, ':')) {
            snprintf(h, PC_ONLINE_ADDR, "%.*s", (int)(colon - s), s);
            *port = atoi(colon + 1);
        } else {
            snprintf(h, PC_ONLINE_ADDR, "%.127s", s);
        }
        size_t n = strlen(h);
        if (n > 1 && h[n - 1] == '.')   /* a name's root dot */
            h[n - 1] = 0;
    }
    return pa == pb && !strcasecmp(ha, hb);
}

int pc_online_recent_add(char *list, size_t size, const char *host, int default_port)
{
    char items[PC_ONLINE_RECENT + 1][PC_ONLINE_ADDR], h[PC_ONLINE_ADDR], out[PC_ONLINE_RECENT * (PC_ONLINE_ADDR + 2)];
    int n, k = 1;
    size_t len;
    if (pc_online_address(host, h, sizeof h, NULL, 0) != 0)
        return -1;
    n = pc_online_recent(list, items + 1, PC_ONLINE_RECENT);
    memcpy(items[0], h, sizeof h);
    for (int i = 1; i <= n; i++) {
        int dup = 0;
        for (int j = 0; j < k && !dup; j++)
            dup = same_host(items[i], items[j], default_port);
        if (!dup && k < PC_ONLINE_RECENT)
            memmove(items[k++], items[i], sizeof items[0]);
    }
    len = 0;
    out[0] = 0;
    for (int i = 0; i < k; i++)
        len += (size_t)snprintf(out + len, sizeof out - len, "%s%s", i ? ", " : "", items[i]);
    if (len + 1 > size)
        return -1;
    memcpy(list, out, len + 1);
    return 0;
}

/* ---------------------------------------------------------------- the restart's command line */

/* main.c's options that take a value (ARG()), so a value is never taken for an option; and those a restart leaves
 * out. --net-host's port is optional: digits after it are its. */
static const char *const with_value[] = {
    "--rom", "--resolution", "--size", "--scale", "--widescreen", "--frame-rate", "--screenshot-at", "--screenshot",
    "--seconds", "--frames", "--until-state", "--press", "--press-file", "--log-physics", "--log-voices",
    "--record-inputs", "--replay-inputs", "--input-delay", "--log-hash", "--hash-report", "--net-join", "--net-delay",
    "--net-code", "--net-bind", "--net-timeout", "--press-via", "--pak", "--saves", "--audio", "--dump-audio",
    "--mods-dir", "--config", "--set", "--preset", "--status", "--log", NULL};
static const char *const left_out[] = {
    "--net-host", "--net-join", "--net-code", "--replay-inputs", "--replay-force", "--record-inputs",
    "--input-delay", "--hash-report", "--press", "--press-file", "--press-via", "--selftest", "--version", "--help",
    "-h", "--print-config", NULL};

static int listed(const char *const *list, const char *a)
{
    for (int i = 0; list[i]; i++)
        if (!strcmp(list[i], a))
            return 1;
    return 0;
}

int pc_online_args(int argc, char *const *argv, const PcRelaunch *r, const char **out, int max, char *buf,
                   size_t bufsize)
{
    int n = 0, rom = 0, clock = 0;
    size_t used = 0;
#define PUT(s) do { if (n + 1 >= max) return -1; out[n++] = (s); } while (0)
    if (argc < 1 || max < 2)
        return -1;
    PUT(argv[0]);
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        int takes = listed(with_value, a) && i + 1 < argc;
        if (!strcmp(a, "--net-return"))
            break;   /* it and the rest: the last restart's own */
        if (!strcmp(a, "--net-host")) {   /* [PORT] */
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
                i++;
            continue;
        }
        if (listed(left_out, a)) {
            i += takes;
            continue;
        }
        if (a[0] != '-' && a[0])
            rom = 1;   /* a bare ROM */
        rom |= !strcmp(a, "--rom") || !strcmp(a, "--no-rom");
        clock |= !strcmp(a, "--fast") || !strcmp(a, "--paced");
        PUT(a);
        if (takes)
            PUT(argv[++i]);
    }
    if (!rom && r->rom && r->rom[0]) {
        PUT("--rom");
        PUT(r->rom);
    }
    for (int i = 0; i < r->nset; i++) {
        int k = snprintf(buf + used, bufsize - used, "%s.%s=%s", r->set[i].section, r->set[i].key, r->set[i].value);
        if (k < 0 || used + (size_t)k + 1 > bufsize)
            return -1;
        PUT("--set");
        PUT(buf + used);
        used += (size_t)k + 1;
    }
    if (r->what != PC_RELAUNCH_OFFLINE && r->back)
        PUT("--net-return");
    switch (r->what) {
    case PC_RELAUNCH_HOST:
        PUT("--net-host");
        PUT(r->a && r->a[0] ? r->a : "24049");
        PUT("--net-code");
        PUT(r->b ? r->b : "");
        break;
    case PC_RELAUNCH_JOIN:
        PUT("--net-join");
        PUT(r->a ? r->a : "");
        PUT("--net-code");
        PUT(r->b ? r->b : "");
        break;
    case PC_RELAUNCH_REPLAY:
        PUT("--replay-inputs");
        PUT(r->a ? r->a : "");
        if (!clock)
            PUT("--paced");   /* real time, as it was played (--fast's course: the same game) */
        break;
    }
#undef PUT
    out[n] = NULL;
    return n;
}

/* ---------------------------------------------------------------- quoting */

static int put(char *out, size_t size, size_t *at, char c)
{
    if (*at + 1 >= size)
        return -1;
    out[(*at)++] = c;
    out[*at] = 0;
    return 0;
}

int pc_online_quote_windows(const char *const *argv, int argc, char *out, size_t size)
{
    size_t at = 0;
    if (!size)
        return -1;
    out[0] = 0;
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (i && put(out, size, &at, ' '))
            return -1;
        if (i == 0) {   /* the program: up to its closing quote, no escapes (a path has no quotes) */
            if (put(out, size, &at, '"'))
                return -1;
            for (const char *p = a; *p; p++)
                if (*p != '"' && put(out, size, &at, *p))
                    return -1;
            if (put(out, size, &at, '"'))
                return -1;
            continue;
        }
        if (a[0] && !strpbrk(a, " \t\n\v\"")) {
            for (const char *p = a; *p; p++)
                if (put(out, size, &at, *p))
                    return -1;
            continue;
        }
        if (put(out, size, &at, '"'))
            return -1;
        for (const char *p = a;; p++) {
            size_t slashes = 0;
            while (*p == '\\')
                slashes++, p++;
            /* backslashes before a quote, or before the closing one, are doubled; elsewhere they are themselves */
            size_t times = !*p ? 2 * slashes : *p == '"' ? 2 * slashes + 1 : slashes;
            for (size_t k = 0; k < times; k++)
                if (put(out, size, &at, '\\'))
                    return -1;
            if (!*p)
                break;
            if (put(out, size, &at, *p))
                return -1;
        }
        if (put(out, size, &at, '"'))
            return -1;
    }
    return (int)at;
}

int pc_online_quote_sh(const char *const *argv, int argc, char *out, size_t size)
{
    size_t at = 0;
    if (!size)
        return -1;
    out[0] = 0;
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        int plain = a[0] != 0;
        for (const char *p = a; *p && plain; p++)
            plain = isalnum((unsigned char)*p) || strchr("@%+=:,./_-", *p);
        if (i && put(out, size, &at, ' '))
            return -1;
        if (plain) {
            for (const char *p = a; *p; p++)
                if (put(out, size, &at, *p))
                    return -1;
            continue;
        }
        if (put(out, size, &at, '\''))
            return -1;
        for (const char *p = a; *p; p++) {
            if (*p == '\'') {   /* end the quote, an escaped quote, start again */
                if (put(out, size, &at, '\'') || put(out, size, &at, '\\') || put(out, size, &at, '\'') ||
                    put(out, size, &at, '\''))
                    return -1;
            } else if (put(out, size, &at, *p)) {
                return -1;
            }
        }
        if (put(out, size, &at, '\''))
            return -1;
    }
    return (int)at;
}
