/* port/app/netplay.c: an online session: the game attached to port/net's lockstep (docs/port/NETPLAY.md "Status",
 * NETWORK.md 10; main's side is netplay.h's flow).
 *
 * The pads source (pc_lockstep.h) over the session, with net_start_thread's service thread doing the network:
 *   - local(n): this PC's controller 1 (local[0]), as live input and the scripts shape it, goes to net_submit for
 *     the port this PC owns (the host port 1, the guest port 2). The library assigns it to retrace n + D, D being
 *     this side's delay (--net-delay, or the adaptive one); --input-delay isn't used.
 *   - pads(n): net_pads, waiting up to WAIT_POLL_MS at a time on the condition the net thread signals when input
 *     arrives; then the gate is asked again at once (pc_pads_signal), pumping the window's events between. Ports 1
 *     and 2 are always connected and 3 and 4 never, whatever pads this PC has (one unplugged changes nothing); no
 *     port has rumble (no accessory follows it in a session). After WAITING_MS without the other PC's input, a
 *     notice over the last frame says so; the quit key (Escape) or closing the window leaves.
 *   - hash(n): the curated state hash, folded to 32 bits, every retrace (net_hash); the full hash (the window, own
 *     storage, the decoupled data) when it was taken (net_window_hash). Retrace 0's window hash is also XORed with
 *     the hash of the must-match text, so two PCs whose setups differ in anything it lists (the paks as installed,
 *     the window's size, a test model) end at retrace 0: "the two PCs' setups differ".
 *
 * The clock. Until the paced clock (the net-clock branch) is merged, a session runs on the unpaced virtual clock
 * (--fast's): deterministic, the network deciding only when retraces happen. The paced clock attaches in three
 * places, marked "the paced clock" below: its epoch is net_start_time (pc_net_start); each retrace adds
 * net_pace_adjust_ns to the interval (src_pads); after a stall it re-bases (net.h's contract, NETWORK.md 1). The
 * session's clock (NetConfig.now_ns) is pc_real_ns, the main loop's own, so the times compare. */
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "app.h"
#include "config.h"
#include "input_internal.h"
#include "net.h"
#include "net_os.h"
#include "netplay.h"
#include "pc_lockstep.h"
#include "ultra.h"

#define WAIT_POLL_MS 4           /* net_pads waits this long at a time; the net thread wakes it when input comes */
#define WAITING_MS 250           /* the "Waiting for ..." notice after this long without the other PC's input */
#define NOTICE_EVERY_NS 250000000u   /* the notice's seconds, redrawn this often */
#define LINGER_MS 500            /* this side leaving: how long the other PC may take to have its last inputs */
#define HOST_WAIT_S 600          /* a host nobody joins gives up after 10 minutes (RUSH2049_TEST_NET_HOST_WAIT: S s) */
#define KEEP_RETRACES 240        /* the curated state kept for a desync's report (4 s) */
#define FULL_EVERY 60            /* the window's hash every second */
#define EXIT_SESSION 3           /* the exit code of a session that ended otherwise than a player leaving */

static struct {
    int role;                    /* PC_NET_* */
    NetSession *s;
    int window;
    uint8_t *pak[2];             /* the session's paks: port 1's (the host's), port 2's (the guest's); NULL none */
    uint64_t setup_hash;         /* the must-match text's compared lines */
    char *header;                /* the input log's header text: the must-match text and the session's '#' lines */
    char base[PC_APP_PATH];      /* netplay/DATE-ROLE: .pads, .p1.mpk, .p2.mpk, .desync.txt */
    char pads_path[PC_APP_PATH + 16], report_path[PC_APP_PATH + 16];
    char quit_key[64];
    PcPadsSource src;
    int notice;                  /* the waiting notice is up */
    uint64_t notice_at;
    int started;                 /* the game started (pc_net_start) */
    int ended;                   /* why the session ended has been said (session_ended) */
    int shown;                   /* the end's message box was shown */
    int paced;                   /* the paced clock (pc_time_paced): time sync's adjustments reach it */
    int64_t pace_ns;             /* time sync's adjustments, summed */
    uint32_t pace_n;
    int code;                    /* a join code is set */
    int joined;                  /* the guest: the host's settings arrived */
    int gave_up;                 /* the host: nobody joined in this many seconds (HOST_WAIT_S) */
    char join[256];              /* the guest: the host as --net-join named it, without its port */
    int end_local;               /* pc_net_finish: the session ended by this side's own choice (NET_END_LOCAL) */
} S;

static const char *peer_name(void)
{
    return S.role == PC_NET_HOST ? "guest" : "host";
}

/* The other PC as the player knows it: its address and port; the guest's host also by the name it was given. */
static void peer_text(char *out, size_t n)
{
    NetAddr a;
    char addr[80] = "";
    int numeric = strspn(S.join, "0123456789.") == strlen(S.join) || strchr(S.join, ':') != NULL;
    if (S.s && net_peer_addr(S.s, &a))
        net_addr_str(&a, addr, sizeof addr);
    if (S.role == PC_NET_GUEST && S.join[0] && addr[0] && !numeric)
        snprintf(out, n, "%s (%s)", S.join, addr);
    else
        snprintf(out, n, "%s", addr[0] ? addr : S.join[0] ? S.join : "?");
}

int pc_net_role(void)
{
    return S.role;
}

/* ---------------------------------------------------------------- small things */

static uint64_t clock_ns(void *user)
{
    (void)user;
    return pc_real_ns();
}

/* The library's lines (called with its lock held: nothing here calls back into it). */
static void net_log(void *user, const char *line)
{
    (void)user;
    fprintf(stderr, "rush2049pc: net: %s\n", line);
}

static void show(const char *what, int window)
{
    if (!S.shown)
        pc_app_alert(window, "Rush 2049 PC: online session", what);
    S.shown = 1;
}

/* Ctrl-C (and the terminal's end) while the session is set up: the main loop's handlers aren't in yet. */
static void on_interrupt(int sig)
{
    (void)sig;
    pc_quit = 1 + 130;
}

static void catch_interrupts(void)
{
    signal(SIGINT, on_interrupt);
    signal(SIGTERM, on_interrupt);
}

/* The session ended before the game: why, logged and shown (on Windows nobody sees a console: the box is all there
 * is), but for the player's own Ctrl-C. The exit code; PC_NET_CANCELLED (exit 0, but no game) when the other player
 * left. */
static int failed(int window)
{
    char msg[256], text[600];
    NetEnd e = net_end_reason(S.s, NULL, msg, sizeof msg);
    if (e == NET_END_NONE)
        snprintf(msg, sizeof msg, "this side left the session");
    if (S.gave_up)
        snprintf(text, sizeof text, "Nobody joined the online race in %d %s, so this PC stopped hosting. Host again "
                 "when your friend is ready to join.", S.gave_up % 60 ? S.gave_up : S.gave_up / 60,
                 S.gave_up % 60 ? "seconds" : "minutes");
    else if (S.role == PC_NET_GUEST && !S.joined)
        snprintf(text, sizeof text, "Couldn't join the online race at %s: %s.", S.join, msg);
    else
        snprintf(text, sizeof text, "The online race ended before it started (%s): %s.", net_end_name(e), msg);
    fprintf(stderr, "rush2049pc: online: %s\n", text);
    show(text, window && (e != NET_END_LOCAL || S.gave_up));
    if (pc_quit > 1)
        return pc_quit - 1;
    return e == NET_END_PEER_LEFT ? PC_NET_CANCELLED : EXIT_SESSION;
}

/* Waits for the session to reach a state (or pass it). 0 when it did; 1 when it ended or this side was told to stop
 * (Ctrl-C), then ended locally. With ui (the window is open, before the game): PC_NET_CANCELLED when the player left
 * (Esc, the window closed), and the notice drawn again meanwhile. */
static int wait_state(NetState want, const char *what, int ui)
{
    uint64_t t0 = pc_real_ns(), said = t0, drawn = 0;
    for (;;) {
        NetState st = net_state(S.s);
        if (st == NET_ENDED)
            return 1;
        if (st >= want)
            return 0;
        if (pc_quit) {
            net_end(S.s, NET_END_LOCAL);
            return 1;
        }
        if (ui && pc_net_events_leave()) {
            net_end_text(S.s, NET_END_LOCAL, "the player left before the start");
            fprintf(stderr, "rush2049pc: online: left before the start (Esc, or the window closed)\n");
            return PC_NET_CANCELLED;
        }
        uint64_t now = pc_real_ns();
        if (ui && now - drawn >= NOTICE_EVERY_NS) {
            pc_gfx_redraw();
            drawn = now;
        }
        if (now - said >= 10000000000u) {
            fprintf(stderr, "rush2049pc: online: %s (%.0f s so far; %s)\n", what, (now - t0) / 1e9,
                    ui ? "Esc in the window leaves" : "Ctrl-C stops");
            said = now;
        }
        net_os_sleep_ns(10000000u);
    }
}

/* ---------------------------------------------------------------- 1. connecting */

int pc_net_connect(const PcNetOptions *o, const PcSettings *cfg, const char *build, const char *blob,
                   const uint8_t *pak, int window)
{
    char err[400], host[256], peer[300];
    NetConfig c;
    S.role = o->role;
    S.window = window;
    net_config_defaults(&c);
    c.role = o->role == PC_NET_HOST ? NET_HOST : NET_GUEST;
    c.ports = o->role == PC_NET_HOST ? 0x1 : 0x2;   /* the host plays controller 1, the guest controller 2 */
    if (o->role == PC_NET_GUEST) {
        char join[256];
        int port;
        size_t n;
        snprintf(join, sizeof join, "%s", o->join + strspn(o->join, " \t"));   /* as typed at a prompt */
        for (n = strlen(join); n && strchr(" \t\r\n", join[n - 1]); n--)
            join[n - 1] = 0;
        if (net_split_hostport(join, host, sizeof host, &port, cfg->net_port) != 0) {
            snprintf(err, sizeof err, "The address to join, '%s', isn't one: it is a name or an address (my-pc, "
                     "100.101.102.103), with :PORT after it when the host's port isn't 24049", join);
            fprintf(stderr, "rush2049pc: %s\n", err);
            show(err, window);
            return 2;
        }
        snprintf(peer, sizeof peer, strchr(host, ':') ? "[%s]:%d" : "%s:%d", host, port);
        snprintf(S.join, sizeof S.join, "%s", host);
        c.peer = peer;
    }
    c.bind = o->bind && *o->bind ? o->bind : NULL;
    c.port = o->role == PC_NET_HOST ? (o->port >= 0 ? o->port : cfg->net_port) : 0;
    c.build = build;
    c.settings = blob;
    c.settings_size = strlen(blob);
    c.files[0].data = pak;
    c.files[0].size = pak ? PC_PAK_SIZE : 0;
    c.nfiles = 1;
    c.delay_fixed = cfg->net_delay;
    if (o->timeout_ms > 0)
        c.timeout_ms = o->timeout_ms;
    c.join_code = cfg->net_code[0] ? cfg->net_code : NULL;
    c.now_ns = clock_ns;
    c.log = net_log;
    if (o->paced && getenv("RUSH_PACE_SPEED") && atof(getenv("RUSH_PACE_SPEED")) > 0)   /* tests: N64 time S x wall */
        c.retrace_ns = (int64_t)(1e9 / PC_VI_HZ / atof(getenv("RUSH_PACE_SPEED")));
    S.paced = o->paced;
    S.code = c.join_code != NULL;
    catch_interrupts();
    S.s = net_create(&c, err, sizeof err);
    if (!S.s || net_start_thread(S.s) != 0) {
        char text[720];
        if (S.s)
            snprintf(err, sizeof err, "no thread for the network");
        if (o->role == PC_NET_GUEST)
            snprintf(text, sizeof text, "Couldn't join the online race at %s: %s.", S.join, err);
        else
            snprintf(text, sizeof text, "Couldn't host an online race on UDP port %d: %s.%s", c.port, err,
                     strstr(err, "in use") ? " Is another copy of the program hosting already?" : "");
        fprintf(stderr, "rush2049pc: online: %s\n", text);
        show(text, window);
        return EXIT_SESSION;
    }
    if (pak && (S.pak[o->role == PC_NET_HOST ? 0 : 1] = malloc(PC_PAK_SIZE)) != NULL)   /* this PC's, as sent */
        memcpy(S.pak[o->role == PC_NET_HOST ? 0 : 1], pak, PC_PAK_SIZE);
    if (o->role == PC_NET_HOST) {
        int port = net_local_port(S.s);
        char suffix[16] = "";
        if (port != NET_DEFAULT_PORT)
            snprintf(suffix, sizeof suffix, ":%d", port);
        fprintf(stderr, "rush2049pc: online: hosting on UDP port %d%s: the guest joins with --net-join ADDRESS%s, "
                "ADDRESS being this PC's Tailscale name or 100.x address (or the router's public address, the port "
                "forwarded to this PC); the waiting screen lists them\n", port, c.join_code ? " (with a join code)" : "",
                suffix);
#ifdef _WIN32
        fprintf(stderr, "rush2049pc: online: Windows may ask once whether to let the program on the network: allow "
                "it (for private networks)\n");
#endif
    } else {
        fprintf(stderr, "rush2049pc: online: joining %s as the guest (controller 2)%s\n", peer,
                c.join_code ? " with a join code" : "");
    }
    if (o->role == PC_NET_HOST)
        return 0;   /* its guest is waited for with the window open (pc_net_wait_guest) */
    if (wait_state(NET_ACCEPT, "waiting for the host", 0) != 0)
        return failed(window);
    S.joined = 1;
    return 0;
}

int pc_net_wait_guest(int window)
{
    uint64_t t0 = pc_real_ns(), said = t0, drawn = 0;
    int r = 0, told = 0, limit = HOST_WAIT_S;
    char peer[128];
    if (getenv("RUSH2049_TEST_NET_HOST_WAIT") && atoi(getenv("RUSH2049_TEST_NET_HOST_WAIT")) > 0)
        limit = atoi(getenv("RUSH2049_TEST_NET_HOST_WAIT"));
    pc_net_screen_open(net_local_port(S.s), S.code);
    for (;;) {
        NetState st = net_state(S.s);
        uint64_t now = pc_real_ns();
        if (st == NET_ENDED) {
            r = failed(window);
            break;
        }
        if (st >= NET_ACCEPT)
            break;
        if (pc_quit) {   /* Ctrl-C */
            net_end_text(S.s, NET_END_LOCAL, "stopped while waiting for a guest");
            r = failed(window);
            break;
        }
        if (pc_net_screen_pump()) {
            net_end_text(S.s, NET_END_LOCAL, "hosting cancelled");
            fprintf(stderr, "rush2049pc: online: hosting cancelled (Esc, or the window closed)\n");
            r = PC_NET_CANCELLED;
            break;
        }
        if (now - t0 > (uint64_t)limit * 1000000000u) {
            S.gave_up = limit;
            net_end_text(S.s, NET_END_LOCAL, "nobody joined in time");
            r = failed(window);
            break;
        }
        if (st >= NET_HANDSHAKE && !told) {   /* its settings and pak on their way */
            char line[192];
            peer_text(peer, sizeof peer);
            snprintf(line, sizeof line, "A player joined from %s: getting their settings...", peer);
            pc_net_screen_status(line);
            told = 1;
        }
        if (pc_net_screen_dirty() || now - drawn >= NOTICE_EVERY_NS) {
            pc_net_screen_draw(now - t0);
            drawn = now;
        }
        if (now - said >= 10000000000u) {
            fprintf(stderr, "rush2049pc: online: waiting for a guest (%.0f s so far; Esc in the window cancels)\n",
                    (now - t0) / 1e9);
            said = now;
        }
        net_os_sleep_ns(10000000u);
    }
    pc_net_screen_close();
    if (!r) {
        peer_text(peer, sizeof peer);
        fprintf(stderr, "rush2049pc: online: a guest joined from %s\n", peer);
    }
    return r;
}

void pc_net_starting(void)
{
    char title[96], peer[128];   /* the notice's title holds 95 characters */
    peer_text(peer, sizeof peer);
    if (S.role == PC_NET_HOST)
        snprintf(title, sizeof title, "A player joined from %.70s.", peer);
    else
        snprintf(title, sizeof title, "Joined the online race at %.65s.", peer);
    pc_menu_notice(title, "Starting when both PCs are ready...  Esc leaves.");
    S.notice = 1;
    S.notice_at = pc_real_ns();
    pc_gfx_redraw();
}

const char *pc_net_host_settings(void)
{
    static char *text;
    const void *data;
    size_t size;
    if (!text && S.s && net_peer_settings(S.s, &data, &size) == 1 && (text = malloc(size + 1)) != NULL) {
        memcpy(text, data, size);
        text[size] = 0;
    }
    return text ? text : "";
}

int pc_net_blob_value(const char *blob, const char *key, char *out, size_t size)
{
    size_t k = strlen(key);
    for (const char *l = blob; l && *l;) {
        const char *end = strchr(l, '\n');
        size_t n = end ? (size_t)(end - l) : strlen(l);
        if (n > k && !strncmp(l, key, k) && l[k] == '=') {
            snprintf(out, size, "%.*s", (int)(n - k - 1), l + k + 1);
            return 0;
        }
        l = end ? end + 1 : NULL;
    }
    if (size)
        out[0] = 0;
    return -1;
}

/* ---------------------------------------------------------------- 2. the host's settings */

int pc_net_adopt(PcSettings *cfg, const char *host, int *mods_inputs, char *why, size_t why_size)
{
    int seen[64] = {0};   /* by pc_must_match's index (18 now) */
    *mods_inputs = 0;
    if (pc_must_match_count > 64) {
        snprintf(why, why_size, "too many settings");
        return -1;
    }
    for (const char *l = host; *l;) {
        const char *end = strchr(l, '\n');
        size_t n = end ? (size_t)(end - l) : strlen(l);
        char line[PC_CFG_PATH + 64], *eq, *dot;
        snprintf(line, sizeof line, "%.*s", (int)(n < sizeof line ? n : sizeof line - 1), l);
        l = end ? end + 1 : l + n;
        if (!line[0] || line[0] == '#')
            continue;
        eq = strchr(line, '=');
        dot = strchr(line, '.');
        if (!eq) {
            snprintf(why, why_size, "the host's settings have a line that isn't key=value");
            return -1;
        }
        *eq = 0;
        if (!dot || dot > eq) {
            if (!strcmp(line, "build") || !strcmp(line, "rom") || !strcmp(line, "mods"))
                continue;   /* the identity: main compares it */
            snprintf(why, why_size, "the host's settings name '%s', which this build doesn't know", line);
            return -1;
        }
        *dot = 0;
        const char *section = line, *key = dot + 1, *value = eq + 1;
        int i;
        for (i = 0; i < pc_must_match_count; i++)
            if (pc_must_match[i].adopt > 0 && !strcasecmp(pc_must_match[i].section, section) &&
                !strcasecmp(pc_must_match[i].key, key))
                break;
        if (i == pc_must_match_count) {
            snprintf(why, why_size, "the host's settings name %s.%s, which a guest doesn't take", section, key);
            return -1;
        }
        char was[PC_CFG_PATH], err[200];
        pc_settings_get(cfg, section, key, was, sizeof was);
        seen[i] = 1;
        if (!strcmp(was, value))
            continue;
        if (pc_settings_set(cfg, section, key, value, err, sizeof err) != 0) {
            snprintf(why, why_size, "the host's %s.%s = %s can't be used here: %s", section, key, value, err);
            return -1;
        }
        fprintf(stderr, "rush2049pc: online: %s.%s %s -> %s (the host's, for this session)\n", section, key, was,
                value);
        if (!strcmp(section, "game") && (!strcmp(key, "memory") || !strcmp(key, "expansion_pak") ||
                                        !strcmp(key, "cars")))
            *mods_inputs = 1;
    }
    for (int i = 0; i < pc_must_match_count; i++)
        if (pc_must_match[i].adopt > 0 && !seen[i]) {
            snprintf(why, why_size, "the host's settings don't say %s.%s", pc_must_match[i].section,
                     pc_must_match[i].key);
            return -1;
        }
    return 0;
}

int pc_net_reject(const char *why, const char *for_peer, int window)
{
    char text[600];
    fprintf(stderr, "rush2049pc: online: refused: %s\n", why);
    net_end_text(S.s, NET_END_REJECTED, for_peer && *for_peer ? for_peer : why);
    if (S.role == PC_NET_GUEST)
        snprintf(text, sizeof text, "Can't join the online race at %s: %s.", S.join, why);
    else
        snprintf(text, sizeof text, "The online race can't start: %s.", why);
    show(text, window);
    return EXIT_SESSION;
}

/* ---------------------------------------------------------------- 3, 4. the paks */

int pc_net_paks(int window)
{
    const void *data[2];
    size_t size[2];
    int mine = S.role == PC_NET_HOST ? 0 : 1;
    char why[200], text[400];
    for (int k = 0; k < 2; k++) {
        if (k == mine) {   /* this side's own: as it sent it (the library keeps no copy for the caller) */
            data[k] = NULL;
            size[k] = 0;
            continue;
        }
        if (net_peer_file(S.s, 0, &data[k], &size[k]) != 1) {
            snprintf(text, sizeof text, "the %s's controller pak didn't arrive", peer_name());
            return pc_net_reject(text, NULL, window);
        }
    }
    for (int k = 0; k < 2; k++) {
        if (k == mine)
            continue;
        if (!size[k]) {
            fprintf(stderr, "rush2049pc: online: the %s's player has no controller pak\n", peer_name());
            continue;
        }
        if (size[k] != PC_PAK_SIZE || pc_pak_image_check(data[k], size[k], why, sizeof why) != PC_PAK_IMAGE_OK) {
            snprintf(text, sizeof text, "the %s's controller pak can't be used: %s", peer_name(), why);
            return pc_net_reject(text, S.role == PC_NET_HOST ? "the guest's controller pak is damaged"
                                                             : "the host's controller pak is damaged", window);
        }
        S.pak[k] = malloc(PC_PAK_SIZE);
        if (!S.pak[k])
            return pc_net_reject("out of memory", NULL, window);
        memcpy(S.pak[k], data[k], PC_PAK_SIZE);
    }
    return 0;
}

void pc_net_install_paks(PcSettings *cfg)
{
    /* Two paks with one ID are one pak moved between ports to the game (docs/port/ULTRA.md "Accessories"): a guest's
     * pak made on its own controller 1 has the host's controller 1's ID. Port 2's copy takes controller 2's own, on
     * both PCs alike (both have both images); the session's paks are never written back. */
    if (S.pak[0] && S.pak[1] && !memcmp(S.pak[0] + 0x20, S.pak[1] + 0x20, 32)) {
        pc_pak_own_id(S.pak[1], 1);
        fprintf(stderr, "rush2049pc: online: the two paks have one ID: port 2's copy takes controller 2's own for the "
                "session\n");
    }
    for (int ch = 0; ch < 4; ch++) {
        pc_si_set_pak_image(ch, ch < 2 ? S.pak[ch] : NULL);
        cfg->pak[ch] = ch < 2 && S.pak[ch] ? PC_CFG_PAK_MEMORY : PC_CFG_PAK_NONE;
    }
    fprintf(stderr, "rush2049pc: online: controller 1 (the host's player) %s, controller 2 (the guest's) %s; in "
            "memory only: nothing is written to the disk during the session\n",
            S.pak[0] ? "has the host's controller pak" : "has no controller pak",
            S.pak[1] ? "has the guest's controller pak" : "has no controller pak");
}

/* ---------------------------------------------------------------- the pads source */

/* Why the session ended while the game ran: said, and a desync's report written (retrace detail's state as hashed,
 * kept for the last KEEP_RETRACES retraces), beside the input log. Once. */
static NetEnd session_ended(void)
{
    char msg[256];
    uint32_t detail = 0;
    NetEnd e = net_end_reason(S.s, &detail, msg, sizeof msg);
    if (S.ended || e == NET_END_NONE)
        return e;
    S.ended = 1;
    if (S.notice)
        pc_menu_notice(NULL, NULL), S.notice = 0;
    if (S.started)
        fprintf(stderr, "rush2049pc: online: the session ended at retrace %u (%s): %s\n", pc_frame, net_end_name(e),
                msg);
    if (e == NET_END_DESYNC) {
        if (detail == 0)
            fprintf(stderr, "rush2049pc: online: at retrace 0: the two PCs' setups differ (each PC's log has its "
                    "must-match text: compare them)\n");
        if (pc_hash_report(S.report_path, detail) == 0)
            fprintf(stderr, "rush2049pc: online: the desync report (retrace %u's state as hashed): %s\n", detail,
                    S.report_path);
        else
            fprintf(stderr, "rush2049pc: online: no desync report: can't write %s\n", S.report_path);
    }
    return e;
}

/* RUSH_SIM_NET's pause (net_frozen: this PC stands still, as a stopped process would) holds this PC's sample too: it
 * is submitted, and so assigned, when the pause ends, as a stopped process's would be. Submitted at once, it would
 * be assigned at the pause's start and sent at its end, and the peer would take the pause for its transit. */
static struct {
    int held;
    uint32_t n;
    NetPad pad;
} Q;

static void src_local(void *user, uint32_t n, const PcPad local[4])
{
    (void)user;
    NetPad p = {local[0].buttons, local[0].stick_x, local[0].stick_y};
    if (net_frozen(S.s)) {
        Q.held = 1, Q.n = n, Q.pad = p;
        return;
    }
    net_submit(S.s, n, &p);   /* -1 once the session has ended: src_pads says why */
}

static int src_pads(void *user, uint32_t n, PcPad out[4])
{
    (void)user;
    NetPad p[NET_MAX_PORTS];
    int frozen = net_frozen(S.s);   /* RUSH_SIM_NET's pause */
    if (!frozen && Q.held) {
        Q.held = 0;
        net_submit(S.s, Q.n, &Q.pad);
    }
    int r = frozen ? 0 : net_pads(S.s, n, p, WAIT_POLL_MS);
    if (r == 1) {
        for (int i = 0; i < 4; i++)
            out[i] = (PcPad){p[i].buttons, p[i].stick_x, p[i].stick_y, i < 2, 0};
        if (S.notice) {
            pc_menu_notice(NULL, NULL);
            S.notice = 0;
        }
        /* Time sync (NETWORK.md 6): retrace n + 1 comes this much later in wall time than a retrace after n
         * (pc_pace_adjust_ns: until changed, so every retrace says it). The loop itself re-bases a retrace whose
         * pads came more than half a retrace late (ULTRA.md 10, port/net's rule). Unpaced (--fast), only counted. */
        int64_t adj = net_pace_adjust_ns(S.s);
        if (S.paced)
            pc_pace_adjust_ns(adj);
        S.pace_ns += adj;
        S.pace_n++;
        return PC_PADS_READY;
    }
    if (r < 0) {   /* the main loop stops: 0 when the other player left, else EXIT_SESSION */
        if (!S.ended && !pc_quit)
            pc_quit = session_ended() == NET_END_PEER_LEFT ? 1 : 1 + EXIT_SESSION;
        pc_pads_signal();
        return PC_PADS_WAIT;
    }
    int ms = net_waiting_ms(S.s);
    uint64_t now = pc_real_ns();
    if (ms >= WAITING_MS && (!S.notice || now - S.notice_at >= NOTICE_EVERY_NS)) {
        char title[96], line[160];
        snprintf(title, sizeof title, "Waiting for the %s...", peer_name());
        snprintf(line, sizeof line, "%.1f s.%s%s%s", ms / 1000.0, S.quit_key[0] ? " " : "", S.quit_key,
                 S.quit_key[0] ? " leaves the session." : " Closing the window leaves the session.");
        pc_menu_notice(title, line);
        pc_gfx_redraw();
        S.notice = 1;
        S.notice_at = now;
    }
    pc_pads_signal();   /* the gate asks again at once: the waiting is net_pads', woken by the net thread */
    return PC_PADS_WAIT;
}

static void src_hash(void *user, uint32_t n, const PcStateHash *h)
{
    (void)user;
    if (S.ended || net_state(S.s) == NET_ENDED)
        return;
    net_hash(S.s, n, (uint32_t)(h->state ^ h->state >> 32));
    if (n == 0 || h->full)   /* retrace 0's: the two setups too */
        net_window_hash(S.s, n, (h->full ? h->full_hash : 0) ^ (n == 0 ? S.setup_hash : 0));
}

/* ---------------------------------------------------------------- 5, 6. the start */

/* The data folder's netplay/DATE-ROLE (a name not taken). */
static void name_files(const char *data_dir)
{
    char dir[PC_APP_PATH], stamp[64];
    time_t now = time(NULL);
    struct tm tm;
#ifdef _WIN32
    tm = *localtime(&now);
#else
    localtime_r(&now, &tm);
#endif
    strftime(stamp, sizeof stamp, "%Y-%m-%d_%H%M%S", &tm);
    pc_app_join(dir, sizeof dir, data_dir, "netplay");
    pc_app_mkdirs(dir);
    for (int k = 1; k < 100; k++) {
        char name[128], probe[PC_APP_PATH + 16];
        snprintf(name, sizeof name, k == 1 ? "%s-%s" : "%s-%s-%d", stamp, S.role == PC_NET_HOST ? "host" : "guest", k);
        pc_app_join(S.base, sizeof S.base, dir, name);
        snprintf(probe, sizeof probe, "%s.pads", S.base);
        if (!pc_app_exists(probe))
            break;
    }
    snprintf(S.pads_path, sizeof S.pads_path, "%s.pads", S.base);
    snprintf(S.report_path, sizeof S.report_path, "%s.desync.txt", S.base);
}

int pc_net_begin(PcLockstepOptions *ls, const char *must_match, const char *data_dir, const char *quit_key,
                 int window)
{
    PcXxh64 x;
    size_t n = strlen(must_match);
    snprintf(S.quit_key, sizeof S.quit_key, "%s", quit_key ? quit_key : "");
    if (ls->replay) {
        fprintf(stderr, "rush2049pc: --replay-inputs plays a recording: not in an online session\n");
        return 2;
    }
    name_files(data_dir);
    /* the setup's hash: the lines both PCs must have alike ('#' lines are only said) */
    pc_xxh64_init(&x, 0);
    for (const char *l = must_match; *l;) {
        const char *end = strchr(l, '\n');
        size_t k = end ? (size_t)(end - l) + 1 : strlen(l);
        if (*l != '#') {
            pc_xxh64_update(&x, l, k);
            fprintf(stderr, "rush2049pc: online: must match: %.*s\n", (int)(end ? k - 1 : k), l);
        }
        l += k;
    }
    S.setup_hash = pc_xxh64_digest(&x);
    fprintf(stderr, "rush2049pc: online: the setup's hash %016llx (both PCs' must be the same: it is retrace 0's)\n",
            (unsigned long long)S.setup_hash);
    /* the input log: every retrace's pads with the must-match text, so the session replays (--replay-inputs) */
    S.header = malloc(n + 512);
    if (!S.header)
        return 1;
    snprintf(S.header, n + 512, "%s# online session: the %s's log (the other PC keeps its own)\n# paks: %s\n",
             must_match, S.role == PC_NET_HOST ? "host" : "guest", S.base);
    ls->must_match = S.header;
    if (!ls->record)
        ls->record = S.pads_path;
    else
        snprintf(S.pads_path, sizeof S.pads_path, "%s", ls->record);
    if (ls->delay) {
        fprintf(stderr, "rush2049pc: --input-delay isn't used in an online session (--net-delay is its delay)\n");
        return 2;
    }
    for (int k = 0; k < 2; k++) {   /* the paks as the session has them, for a replay */
        char path[PC_APP_PATH + 16];
        snprintf(path, sizeof path, "%s.p%d.mpk", S.base, k + 1);
        if (S.pak[k] && pc_file_write_atomic(path, S.pak[k], PC_PAK_SIZE) != 0)
            fprintf(stderr, "rush2049pc: online: can't write %s (the session can't be replayed without it)\n", path);
    }
    fprintf(stderr, "rush2049pc: online: the session's input log: %s\n", S.pads_path);
    /* the state hash: the curated state every retrace, the window every FULL_EVERY (--log-hash's own when given) */
    if (!pc_hash_enabled())
        pc_hash_enable(FULL_EVERY);
    pc_hash_keep(KEEP_RETRACES);
    pc_input_set_session(1);   /* nothing holds the game; the overlay's locks */
    pc_menu_set_online(pc_net_status);   /* F1's Online rows */
    pc_gfx_hud_dx_off(1);      /* the session's rules (docs/port/LOCKSTEP.md 6) */
    S.src = (PcPadsSource){"online session", NULL, src_local, src_pads, src_hash};
    if (pc_pads_set_source(&S.src) != 0) {
        fprintf(stderr, "rush2049pc: online: the pads have another source\n");
        return 2;
    }
    (void)window;
    return 0;
}

int pc_net_start(int window)
{
    uint64_t start;
    int r;
    net_accept(S.s);
    r = wait_state(NET_STARTING, S.role == PC_NET_HOST ? "waiting for the guest to be ready"
                                                       : "waiting for the host to be ready", 1);
    if (r == PC_NET_CANCELLED)
        return r;
    if (r != 0)
        return failed(window);
    if (!net_start_time(S.s, &start))
        return failed(window);
    /* The paced clock's epoch: N64 time 0 at the agreed start, on pc_real_ns's clock (the session's), so both PCs'
     * retrace N comes at start + N retraces; its paced wait holds the boot until then. Unpaced (--fast), both only
     * start here together. */
    uint64_t now = pc_real_ns();
    fprintf(stderr, "rush2049pc: online: starting in %.0f ms, the input delay %d to start\n",
            start > now ? (start - now) / 1e6 : 0.0, net_delay(S.s));
    if (S.paced)
        pc_pace_set_start(start);
    else if (start > now)
        net_os_sleep_ns(start - now);
    S.started = 1;
    return 0;
}

/* ---------------------------------------------------------------- 7. the end */

/* The session's input log replays offline as it is: --replay-inputs takes a session's settings, rules and paks from
 * the log and the .p1.mpk and .p2.mpk beside it (port/app/netmenu.c), in memory; F1's Online page watches it too. */
static void replay_hint(void)
{
    fprintf(stderr, "rush2049pc: online: to replay the session (with the same build, ROM and mods): rush2049pc "
            "--replay-inputs %s [--paced or --fast] [--log-hash 1,%d]; or F1, Online, Watch a replay\n", S.pads_path,
            FULL_EVERY);
}

int pc_net_finish(int code, int window)
{
    char msg[256], line[600], text[400];
    uint32_t detail = 0;
    NetStats st;
    PcPadsStats ps;
    if (!S.s)
        return code;
    if (S.notice)
        pc_menu_notice(NULL, NULL);
    NetEnd e = net_end_reason(S.s, &detail, msg, sizeof msg);
    if (e == NET_END_NONE) {   /* this side stopped: a limit, the window closed, the quit key, the overlay's Quit */
        if (S.started && !net_linger(S.s, LINGER_MS))
            fprintf(stderr, "rush2049pc: online: the %s may not have this side's last inputs\n", peer_name());
        net_end(S.s, NET_END_LOCAL);
        e = net_end_reason(S.s, &detail, msg, sizeof msg);
    }
    S.end_local = e == NET_END_LOCAL;
    if (e != NET_END_LOCAL)
        session_ended();   /* while the main loop stopped for its own reason: the report all the same */
    net_stats(S.s, &st);
    net_stats_line(&st, line, sizeof line);
    fprintf(stderr, "rush2049pc: online: ended (%s): %s\n", net_end_name(e), msg);
    fprintf(stderr, "rush2049pc: online: %s\n", line);
    pc_pads_stats(&ps);
    fprintf(stderr, "rush2049pc: online: %u retraces; %u waited for the %s's input (%.1f ms in all, the longest %.1f "
            "ms); time sync %s %u of them by %.1f ms in all\n", ps.retraces, ps.waited, peer_name(), ps.wait_ns / 1e6,
            ps.longest_ns / 1e6, S.paced ? "stretched" : "would have stretched (unpaced)",
            (unsigned)st.sync_corrections, S.pace_ns / 1e6);
    if (S.header) {
        fprintf(stderr, "rush2049pc: online: the session's input log: %s\n", S.pads_path);
        replay_hint();
    }
    if (e != NET_END_LOCAL && e != NET_END_PEER_LEFT) {
        snprintf(text, sizeof text, "The online session ended (%s): %s.%s", net_end_name(e), msg,
                 e == NET_END_DESYNC ? "\n\nThe log says where the input log and the desync report are." : "");
        show(text, window);
        if (code == 0)
            code = EXIT_SESSION;
    } else if (e == NET_END_PEER_LEFT) {
        snprintf(text, sizeof text, "The %s left the online session.", peer_name());
        show(text, window);
    }
    pc_menu_set_online(NULL);
    net_destroy(S.s);
    S.s = NULL;
    pc_input_set_session(0);
    return code;
}

int pc_net_end_local(void)
{
    return S.end_local;
}

void pc_net_status(char *peer, size_t peer_size, char *link, size_t link_size)
{
    NetStats st;
    char who[128], hash[48];
    if (!S.s) {
        snprintf(peer, peer_size, "Online: no session");
        snprintf(link, link_size, "%s", "");
        return;
    }
    net_stats(S.s, &st);
    peer_text(who, sizeof who);
    snprintf(peer, peer_size, "Online: the %s, %s", peer_name(), who);
    if (st.hash_checks)   /* the retraces compared and alike, from 0 on: the last of them */
        snprintf(hash, sizeof hash, "hash ok at %llu", (unsigned long long)st.hash_checks - 1);
    else
        snprintf(hash, sizeof hash, "no hash yet");
    snprintf(link, link_size, "RTT %.0f ms, delay %d, stalls %llu, %s", st.rtt_ms, st.delay,
             (unsigned long long)st.stalls, hash);
}
