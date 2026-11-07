/* port/app/netmenu.c: the F1 Online page's main side (netmenu.h; docs/port/NETPLAY.md 13), and an online session's
 * input log replayed offline as the session ran.
 *
 * The page (port/input/menu.c) hosts, joins and watches by restarting the program, since lockstep starts at boot (the
 * handshake comes before the game's memory is mapped). Its act here only takes the choice down; the overlay stops the
 * main loop as its Quit does, main ends the run as always (the session's BYE, the saves, the window, the log) and then
 * pc_net_menu_restart runs the program again with this run's options (port/config/online.c leaves out what the restart
 * replaces) and the restart's own, and the environment the program started with: pc_app_restart (port/app/startup.c),
 * which a guest's rebuild after a desync uses too:
 *   - Linux: execve of /proc/self/exe, the same process: its parent (a shell, a launcher, a test) waits on as before;
 *   - Windows: CreateProcess of the executable's own path, the command line quoted as the C runtime parses it. When this
 *     run had a console or its output went to a file or a pipe (a launcher's .bat, a script, a test), the new program
 *     gets the same standard handles and this one waits for it and exits with its exit code: the launcher's `if
 *     errorlevel 1 pause` and a console that stays open until the game ends keep working, as exec does. Started from
 *     Explorer (nobody sees its output), this one exits at once.
 * A run the page started has --net-return: when its session ends, or doesn't start, otherwise than by the player
 * stopping the program, it starts again as it was (without the session), so the player is back in the game. */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "app.h"
#include "config.h"
#include "input_internal.h"
#include "netaddr.h"
#include "netmenu.h"
#include "netrouter.h"
#include "netplay.h"
#include "pc_lockstep.h"
#include "pc_online.h"
#include "ultra.h"

#define MAX_SETS 60
#define MAX_ARGS 512
#define LIST_FILES 512   /* input logs looked at for the list: the newest, by their names */

static struct {
    int argc;
    char **argv;
    int back;                         /* --net-return: a run the page started */
    int mode;                         /* PC_NET_MENU_* */
    char data[PC_APP_PATH];
    const PcSettings *cfg;
    char rom[PC_APP_PATH];            /* the ROM loaded */
    char build[160], rom_sha[48];     /* this run's, as an input log's header says them */
    int asked, what;                  /* a restart: PC_RELAUNCH_* */
    char a[PC_APP_PATH], b[PC_ONLINE_CODE];
    PcSettingValue set[MAX_SETS + 1];
    int nset;
    char fullscreen[4];
    PcNetAddrs addrs;
    int addrs_on;
    const char *args[MAX_ARGS];       /* the restart's command line (pc_net_menu_restart_say) */
    char argbuf[16384];
    int nargs;
} NM;

/* ---------------------------------------------------------------- an input log's header */

#define HEAD_FIXED 36u   /* port/ultra/replay.c's fixed header */

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* The must-match text of an input log, malloc'ed (NULL: not one), and its retraces. */
static char *log_header(const char *path, uint32_t *retraces)
{
    uint8_t head[HEAD_FIXED];
    char *text = NULL;
    struct stat st;
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fread(head, 1, sizeof head, f) == sizeof head && !memcmp(head, PC_PADFILE_MAGIC, 8) &&
        get32(head + 8) == PC_PADFILE_VERSION && get32(head + 32) < (1u << 20) && get32(head + 16) >= 1 &&
        (text = malloc(get32(head + 32) + 1)) != NULL) {
        uint32_t t = get32(head + 32);
        if (fread(text, 1, t, f) != t) {
            free(text);
            text = NULL;
        } else {
            text[t] = 0;
            if (retraces && fstat(fileno(f), &st) == 0 && (uint64_t)st.st_size >= get32(head + 12))
                *retraces = (uint32_t)(((uint64_t)st.st_size - get32(head + 12)) / get32(head + 16));
        }
    }
    fclose(f);
    return text;
}

/* path without its .pads: where its .p1.mpk and .p2.mpk are. */
static void log_base(const char *path, char *out, size_t size)
{
    size_t n = strlen(path);
    if (n > 5 && !strcasecmp(path + n - 5, ".pads"))
        n -= 5;
    snprintf(out, size, "%.*s", (int)n, path);
}

/* The session's pak k (0, 1) beside its log, checked against the header's hash: 1 into img, 0 when the session had
 * none, -1 (why) when it is missing or another. */
static int log_pak(const char *path, const char *header, int k, uint8_t *img, char *why, size_t why_size)
{
    char key[16], want[32], base[PC_APP_PATH], file[PC_APP_PATH + 16], have[32];
    FILE *f;
    snprintf(key, sizeof key, "pak%d.image", k + 1);
    if (pc_net_blob_value(header, key, want, sizeof want) != 0 || !strcmp(want, "none"))
        return 0;
    log_base(path, base, sizeof base);
    snprintf(file, sizeof file, "%s.p%d.mpk", base, k + 1);
    f = fopen(file, "rb");
    if (!f || fread(img, 1, PC_PAK_SIZE, f) != PC_PAK_SIZE) {
        if (f)
            fclose(f);
        snprintf(why, why_size, "controller %d's pak of the session isn't beside its input log (%s)", k + 1, file);
        return -1;
    }
    fclose(f);
    snprintf(have, sizeof have, "%016llx", (unsigned long long)pc_xxh64(img, PC_PAK_SIZE, 0));
    if (strcmp(have, want)) {
        snprintf(why, why_size, "%s isn't the pak the session had (its hash %s, the log's %s)", file, have, want);
        return -1;
    }
    return 1;
}

/* Whether this run can watch the log: its build and ROM, a session's (or a recording's) paks there. 1, or 0 with why. */
static int watchable(const char *path, const char *header, char *why, size_t why_size)
{
    char v[160];
    uint8_t *img = malloc(PC_PAK_SIZE);
    int ok = 1;
    if (pc_net_blob_value(header, "build", v, sizeof v) == 0 && strcmp(v, NM.build))
        snprintf(why, why_size, "another build made it (%.12s; this is %.12s)", v, NM.build), ok = 0;
    else if (pc_net_blob_value(header, "rom", v, sizeof v) == 0 && NM.rom_sha[0] && strcmp(v, NM.rom_sha))
        snprintf(why, why_size, "another ROM made it (SHA-1 %.12s...)", v), ok = 0;
    else if (!strstr(header, "\nsession.rules=on\n"))
        snprintf(why, why_size, "not an online session's log"), ok = 0;
    for (int k = 0; ok && k < 2; k++)
        if (!img || log_pak(path, header, k, img, why, why_size) < 0)
            ok = 0;
    free(img);
    return ok;
}

/* ---------------------------------------------------------------- the page's side */

static int ui_mode(void)
{
    return NM.mode == PC_NET_MENU_SESSION ? PC_ONLINE_SESSION
           : NM.mode == PC_NET_MENU_REPLAY ? PC_ONLINE_REPLAY : PC_ONLINE_OFFLINE;
}

static int ui_addresses(PcOnlineAddr *out, int max, int *pending)
{
    int n = 0, lookup;
    char ip[64];
    if (!NM.addrs_on) {   /* once a run: the lookup's thread writes into it */
        pc_net_addrs_find(&NM.addrs);
        NM.addrs_on = 1;
    }
    lookup = pc_net_addrs_lookup(&NM.addrs);
    *pending = lookup == 1;
    if (n < max && pc_net_router_external(NULL, 0, ip, sizeof ip, NULL) == 0) {   /* hosting, the router's port open */
        snprintf(out[n].text, sizeof out[n].text, "%s", ip);
        snprintf(out[n].tag, sizeof out[n].tag, "internet: the router's");
        n++;
    }
    for (int i = 0; i < NM.addrs.naddr && n < max; i++) {
        const NetLocalAddr *a = &NM.addrs.addr[NM.addrs.order[i]];
        if (pc_net_addrs_rank(a) >= 90)
            continue;   /* containers' and virtual machines' bridges: the log has them */
        net_addr_ip(&a->addr, ip, sizeof ip);
        snprintf(out[n].text, sizeof out[n].text, "%s", ip);
        snprintf(out[n].tag, sizeof out[n].tag, "%s", pc_net_addrs_tag(a));
        n++;
        if (lookup == 2 && NM.addrs.magic[0] && NM.addrs.ts_any && net_addr_equal(&a->addr, &NM.addrs.ts4) &&
            n < max) {   /* its MagicDNS name, under it */
            snprintf(out[n].text, sizeof out[n].text, "%.95s", NM.addrs.magic);
            snprintf(out[n].tag, sizeof out[n].tag, "Tailscale name");
            n++;
        }
    }
    return n;
}

static int by_name_desc(const void *a, const void *b)
{
    return strcmp(*(char *const *)b, *(char *const *)a);
}

/* One log of the list: its label from its name (DATE_TIME-ROLE), its length, whether it can be watched. */
static void replay_row(const char *dir, const char *name, int kept, PcOnlineReplay *r)
{
    int y, mo, d, h, mi, sec;
    char role[16] = "", why[200];
    uint32_t retraces = 0;
    char *header;
    memset(r, 0, sizeof *r);
    pc_app_join(r->file, sizeof r->file, dir, name);
    if (sscanf(name, "%4d-%2d-%2d_%2d%2d%2d-%15[a-z]", &y, &mo, &d, &h, &mi, &sec, role) == 7) {
        snprintf(r->label, sizeof r->label, "%02d-%02d %02d:%02d %s", mo, d, h, mi, role);
        snprintf(r->when, sizeof r->when, "%04d-%02d-%02d %02d:%02d:%02d", y, mo, d, h, mi, sec);
    } else {
        snprintf(r->label, sizeof r->label, "%.31s", name);
        snprintf(r->when, sizeof r->when, "-");
    }
    header = log_header(r->file, &retraces);
    snprintf(r->length, sizeof r->length, "%u:%02u", retraces / 60 / 60, retraces / 60 % 60);
    r->kept = kept;
    if (!header) {
        snprintf(r->info, sizeof r->info, "can't: not an input log this build reads");
        return;
    }
    r->usable = watchable(r->file, header, why, sizeof why);
    if (r->usable)
        snprintf(r->info, sizeof r->info, "the %s's log%s; %u retraces", role[0] ? role : "session",
                 kept ? ", kept" : "", retraces);
    else
        snprintf(r->info, sizeof r->info, "can't: %.86s", why);
    free(header);
}

static int ui_replays(PcOnlineReplay *out, int max)
{
    static const char *const sub[2] = {"netplay", "replays"};
    char *names[LIST_FILES], dir[2][PC_APP_PATH];
    int n = 0, rows = 0;
    for (int k = 0; k < 2; k++) {   /* each NAME with its folder after it: 0x02 netplay/, 0x01 replays/ */
        DIR *d;
        struct dirent *e;
        pc_app_join(dir[k], sizeof dir[k], NM.data, sub[k]);
        if (!(d = opendir(dir[k])))
            continue;
        while ((e = readdir(d)) && n < LIST_FILES) {
            size_t len = strlen(e->d_name);
            if (len > 5 && !strcmp(e->d_name + len - 5, ".pads") && (names[n] = malloc(len + 2)) != NULL) {
                memcpy(names[n], e->d_name, len);
                names[n][len] = (char)(k ? 0x01 : 0x02);
                names[n][len + 1] = 0;
                n++;
            }
        }
        closedir(d);
    }
    /* the newest first: a name starts with its date and time; a session's own log before its kept copy */
    qsort(names, (size_t)n, sizeof *names, by_name_desc);
    for (int i = 0; i < n; i++) {
        size_t len = strlen(names[i]);
        int k = names[i][len - 1] == 0x01;
        names[i][len - 1] = 0;
        if (rows < max)
            replay_row(dir[k], names[i], k, &out[rows++]);
        free(names[i]);
    }
    return rows;
}

/* A copy of the log and its paks in the data folder's replays/, under its own name (the date, the time, the role). */
static int keep(const char *path, char *msg, size_t size)
{
    char dir[PC_APP_PATH], base[PC_APP_PATH], to[PC_APP_PATH + 16], from[PC_APP_PATH + 16];
    const char *name;
    int copied = 0;
    pc_app_join(dir, sizeof dir, NM.data, "replays");
    log_base(path, base, sizeof base);
    name = base + strlen(base);
    while (name > base && name[-1] != '/' && name[-1] != '\\')
        name--;
    if (pc_app_mkdirs(dir) != 0) {
        snprintf(msg, size, "Can't make %.120s", dir);
        return -1;
    }
    for (int k = 0; k < 3; k++) {
        static const char *const ext[3] = {".pads", ".p1.mpk", ".p2.mpk"};
        FILE *f;
        long len;
        char *data;
        snprintf(from, sizeof from, "%s%s", base, ext[k]);
        snprintf(to, sizeof to, "%s%c%s%s", dir, PC_APP_SEP, name, ext[k]);
        if (k == 0 && pc_app_exists(to)) {
            snprintf(msg, size, "Kept already: replays/%.60s.pads", name);
            return -1;
        }
        if (!(f = fopen(from, "rb")))
            continue;   /* a session without that pak */
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);
        data = len >= 0 ? malloc((size_t)len + 1) : NULL;
        if (!data || fread(data, 1, (size_t)len, f) != (size_t)len || pc_file_write_atomic(to, data, (size_t)len) != 0) {
            fclose(f);
            free(data);
            snprintf(msg, size, "Can't copy it to %.120s", to);
            return -1;
        }
        fclose(f);
        free(data);
        copied++;
    }
    fprintf(stderr, "rush2049pc: online page: kept %s in %s (%d files)\n", name, dir, copied);
    snprintf(msg, size, "Kept: replays/%.60s.pads", name);
    return 0;
}

/* What the page chose: the restart taken down (or a log kept). */
static int ui_act(int action, const char *a, const char *b, char *msg, size_t size)
{
    char why[300], *header;
    switch (action) {
    case PC_ONLINE_KEEP:
        return keep(a ? a : "", msg, size);
    case PC_ONLINE_WATCH:
        if (!a || !(header = log_header(a, NULL))) {
            snprintf(msg, size, "Can't read that input log");
            return -1;
        }
        if (!watchable(a, header, why, sizeof why)) {
            free(header);
            snprintf(msg, size, "Can't watch it: %.140s", why);
            return -1;
        }
        free(header);
        NM.what = PC_RELAUNCH_REPLAY;
        snprintf(msg, size, "Restarting to watch the replay...");
        break;
    case PC_ONLINE_HOST:
        NM.what = PC_RELAUNCH_HOST;
        snprintf(msg, size, "Restarting to host on port %.8s...", a ? a : "?");
        break;
    case PC_ONLINE_JOIN:
        NM.what = PC_RELAUNCH_JOIN;
        snprintf(msg, size, "Restarting to join %.60s...", a ? a : "?");
        break;
    case PC_ONLINE_BACK:
        NM.what = PC_RELAUNCH_OFFLINE;
        snprintf(msg, size, "%s", NM.mode == PC_NET_MENU_SESSION ? "Leaving the session..."
                                  : "Restarting without the replay...");
        break;
    default:
        snprintf(msg, size, "?");
        return -1;
    }
    snprintf(NM.a, sizeof NM.a, "%s", a ? a : "");
    snprintf(NM.b, sizeof NM.b, "%s", b ? b : "");
    /* the overlay's changes not saved go along, and fullscreen as F11 left it */
    NM.nset = pc_menu_unsaved_list(NM.set, MAX_SETS);
    int fs = pc_input_live_is_fullscreen();
    if (fs >= 0 && NM.cfg && fs != NM.cfg->fullscreen) {
        int there = 0;
        for (int i = 0; i < NM.nset; i++)
            there |= !strcasecmp(NM.set[i].section, "video") && !strcasecmp(NM.set[i].key, "fullscreen");
        snprintf(NM.fullscreen, sizeof NM.fullscreen, "%s", fs ? "on" : "off");
        if (!there)
            NM.set[NM.nset++] = (PcSettingValue){"video", "fullscreen", NM.fullscreen};
    }
    NM.asked = 1;
    return 0;
}

static const PcOnlineUi g_ui = {ui_mode, ui_addresses, ui_replays, ui_act};

void pc_net_menu_args(int argc, char **argv, int net_return)
{
    NM.argc = argc, NM.argv = argv, NM.back = net_return;
}

void pc_net_menu_install(int mode, const char *data_dir, const PcSettings *cfg, const char *rom, const char *build,
                         const char *rom_sha1)
{
    NM.mode = mode;
    NM.cfg = cfg;
    snprintf(NM.data, sizeof NM.data, "%s", data_dir ? data_dir : "");
    snprintf(NM.rom, sizeof NM.rom, "%s", rom ? rom : "");
    snprintf(NM.build, sizeof NM.build, "%s", build ? build : "");
    snprintf(NM.rom_sha, sizeof NM.rom_sha, "%s", rom_sha1 ? rom_sha1 : "");
    pc_menu_set_online_ui(&g_ui);
}

void pc_net_menu_remember(PcSettings *cfg, const char *address)
{
    char list[PC_CFG_PATH], why[PC_CFG_PATH + 64];
    PcSettingValue ch;
    snprintf(list, sizeof list, "%s", cfg->net_recent);
    if (!cfg->source[0] || pc_online_recent_add(list, sizeof list, address, cfg->net_port) != 0 ||
        !strcmp(list, cfg->net_recent))
        return;
    ch = (PcSettingValue){"net", "recent", list};
    if (pc_settings_save(cfg->source, &ch, 1, why, sizeof why) != 0) {
        fprintf(stderr, "rush2049pc: online: the recent hosts not kept: %s\n", why);
        return;
    }
    snprintf(cfg->net_recent, sizeof cfg->net_recent, "%s", list);
    fprintf(stderr, "rush2049pc: online: %s is first in net.recent (the F1 Online page's hosts joined before)\n",
            address);
}

void pc_net_menu_session_end(int started)
{
    if (!NM.back || NM.asked)
        return;
    if (pc_net_reexec_wanted())
        return;   /* the guest rebuilds after a desync: the same command again (its --net-return too), not the game */
    if (pc_net_window_closed() || pc_quit == 1 + 130 || (started && pc_net_end_local()))
        return;   /* the player stopped the program: the window, Ctrl-C, the quit key or Quit in the game */
    NM.asked = 1;
    NM.what = PC_RELAUNCH_OFFLINE;
    fprintf(stderr, "rush2049pc: online: the session %s: back to the game (--net-return)\n",
            started ? "ended" : "didn't start");
}

int pc_net_menu_restart_asked(void)
{
    return NM.asked;
}

void pc_net_menu_restart_say(void)
{
    char line[8192];
    PcRelaunch r = {NM.what, NM.a, NM.b, NM.rom, 1, NM.set, NM.nset};
    if (!NM.asked || !NM.argc)
        return;
    NM.nargs = pc_online_args(NM.argc, NM.argv, &r, NM.args, MAX_ARGS, NM.argbuf, sizeof NM.argbuf);
    if (NM.nargs < 0) {
        fprintf(stderr, "rush2049pc: can't restart: the command line is too long\n");
        return;
    }
    pc_online_quote_sh(NM.args, NM.nargs, line, sizeof line);
    fprintf(stderr, "rush2049pc: restarting: %s\n", line);
}

int pc_net_menu_restart(int code)
{
    if (!NM.asked || NM.nargs <= 0)
        return code;
    /* this run's options changed (pc_online_args), with the environment the program started with: a new run takes
     * RUSH_GFX_* as a layer over its settings file, and main set them from the settings in effect (a guest's, the
     * host's draw distance); exec on Linux, CreateProcess on Windows (port/app/startup.c) */
    return pc_app_restart(NM.args, NM.nargs, PC_APP_RESTART_START_ENV);
}

/* ---------------------------------------------------------------- a session's log replayed offline */

static struct {
    uint8_t *pak[2];
    int rumble[2];   /* that port had a rumble pak (net.rumble: the header's game.pak or pak2 = rumble) */
    int on;
} RS;

int pc_replay_session(const char *file, PcSettings *cfg, char *why, size_t why_size)
{
    char *header = log_header(file, NULL), v[PC_CFG_PATH], was[PC_CFG_PATH], err[200];
    int changed = 0;
    if (!header)
        return 0;   /* the replay says what is wrong with it */
    if (!strstr(header, "\nsession.rules=on\n")) {
        free(header);
        return 0;
    }
    for (int i = 0; i < pc_must_match_count; i++) {   /* the settings the session ran with */
        char key[64];
        const PcMustMatch *m = &pc_must_match[i];
        if (m->adopt < 0)
            continue;   /* each PC's own in a session (audio.latency: the paced audio's default is the header's) */
        snprintf(key, sizeof key, "%s.%s", m->section, m->key);
        if (pc_net_blob_value(header, key, v, sizeof v) != 0)
            continue;   /* the replay's check says it */
        pc_settings_get(cfg, m->section, m->key, was, sizeof was);
        if (!strcmp(was, v))
            continue;
        if (pc_settings_set(cfg, m->section, m->key, v, err, sizeof err) != 0) {
            snprintf(why, why_size, "the session's %s = %s can't be used here: %s", key, v, err);
            free(header);
            return -1;
        }
        fprintf(stderr, "rush2049pc: replay: %s %s -> %s (the session's)\n", key, was, v);
        changed++;
    }
    for (int k = 0; k < 2; k++) {   /* the session's paks, beside its log */
        uint8_t *img = malloc(PC_PAK_SIZE);
        int r = img ? log_pak(file, header, k, img, why, why_size) : -1;
        if (r <= 0)
            free(img);
        if (r < 0) {
            if (!img)
                snprintf(why, why_size, "out of memory");
            free(header);
            return -1;
        }
        RS.pak[k] = r ? img : NULL;
    }
    for (int k = 0; k < 2; k++)   /* net.rumble's: the header's game.pak or pak2 = rumble (taken above) */
        RS.rumble[k] = cfg->pak[k] == PC_CFG_PAK_RUMBLE && !RS.pak[k];
    for (int k = 0; k < 4; k++)   /* as the header's game.pak... say: the session's paks, ports 3 and 4 none */
        cfg->pak[k] = k < 2 && RS.rumble[k] ? PC_CFG_PAK_RUMBLE
                      : k < 2 && RS.pak[k] ? PC_CFG_PAK_MEMORY : PC_CFG_PAK_NONE;
    RS.on = 1;
    fprintf(stderr, "rush2049pc: replay: an online session's input log: its settings (%d changed for this run), its "
            "rules and its paks (controller 1 %s, controller 2 %s), in memory only\n", changed,
            RS.rumble[0] ? "a rumble pak" : RS.pak[0] ? "a pak" : "none",
            RS.rumble[1] ? "a rumble pak" : RS.pak[1] ? "a pak" : "none");
    free(header);
    return 1;
}

void pc_replay_session_paks(void)
{
    for (int ch = 0; ch < 4; ch++) {
        pc_si_set_pak_image(ch, ch < 2 ? RS.pak[ch] : NULL);
        if (ch < 2 && RS.rumble[ch])
            pc_si_set_pak(ch, PC_PAK_RUMBLE);
    }
}
