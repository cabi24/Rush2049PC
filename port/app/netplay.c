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
 * Rejoining (docs/port/NETPLAY.md 15, NETWORK.md 12). A running session waits net.rejoin seconds (300) for a player
 * that went silent, showing how long. Each side keeps a ticket, netplay/rejoin-ROLE.txt in the data folder (the
 * session's id), refreshed every 20 s and deleted when the session ends; a run that starts with a fresh ticket asks to
 * rejoin. Rejoining, this side gets the session's settings, both paks as they were at its start and the log, adopts
 * them before its memory is mapped (the host too), and replays the log from retrace 0 unpaced with nothing drawn or
 * played (pc_catchup_set), each retrace's hash checked against the other side's; its first live retrace is where the
 * other side waits. A desync with net.recover on: the host has the guest rebuild that way, the guest's program
 * starting again (pc_net_reexec_wanted); the same desync again in the replay is deterministic, and ends the session.
 *
 * The clock. Until the paced clock (the net-clock branch) is merged, a session runs on the unpaced virtual clock
 * (--fast's): deterministic, the network deciding only when retraces happen. The paced clock attaches in three
 * places, marked "the paced clock" below: its epoch is net_start_time (pc_net_start); each retrace adds
 * net_pace_adjust_ns to the interval (src_pads); after a stall it re-bases (net.h's contract, NETWORK.md 1). The
 * session's clock (NetConfig.now_ns) is pc_real_ns, the main loop's own, so the times compare. */
#define _GNU_SOURCE
#include <signal.h>   /* SIGTERM: the system ending the program is its game going, not its player leaving */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "app.h"
#include "config.h"
#include "pc_compat.h"
#include "input_internal.h"
#include "net.h"
#include "net_os.h"
#include "netkeep.h"
#include "net_nat.h"
#include "netplay.h"
#include "netrouter.h"
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
#define TICKET_EVERY_NS 20000000000ull   /* the rejoin ticket's refresh */
#define KNOCK_WAIT_NS 3000000000ull      /* a host with a ticket: how long its guest has to knock (it does every 0.25 s) */
#define CATCHUP_SAY_NS 2000000000ull     /* the replay's progress in the log */

static struct {
    int role;                    /* PC_NET_* */
    NetSession *s;
    int window;
    uint8_t *pak[2];             /* the session's paks: port 1's (the host's), port 2's (the guest's); NULL none */
    int rumble[2];               /* ... or that port's rumble pak (net.rumble: PC_NET_RUMBLE_FILE) */
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
    /* rejoining */
    int window_s;                /* net.rejoin: the rejoin window (0 off) */
    int rejoin;                  /* this side rebuilds the session by replay */
    int catchup;                 /* the replay runs */
    uint32_t live;               /* ... to this retrace */
    uint64_t catch_t0, catch_said;
    char ticket[PC_APP_PATH + 32];
    char build[160];
    uint64_t ticket_at;          /* last written (pc_real_ns) */
    struct {
        uint64_t session;
        uint32_t recover_at;     /* the desync this run rebuilds from (NET_NONE) */
        int recovers;
        long long written;
    } tk;
    int reexec;                  /* the guest rebuilds after a desync: the program starts again */
    int keep;                    /* net.keep: the sessions netplay/ keeps (0 all) */
    uint32_t reported;           /* the host: the desync whose report was written (NET_NONE none) */
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

int pc_net_rejoining(void)
{
    return S.rejoin;
}

int pc_net_reexec_wanted(void)
{
    return S.reexec;
}

/* ---------------------------------------------------------------- the rejoin ticket */

/* netplay/rejoin-ROLE.txt: the session this PC plays (its id), so a run after a crash, a closed window or a reboot can
 * rejoin it. key=value lines. */
static void ticket_write(void)
{
    char text[600], at[16] = "none";
    uint64_t id = S.s ? net_session_id(S.s) : 0;
    if (!S.window_s || !S.ticket[0] || !id)
        return;
    S.tk.session = id;
    if (S.tk.recover_at != NET_NONE)
        snprintf(at, sizeof at, "%u", S.tk.recover_at);
    snprintf(text, sizeof text, "# Rush2049PC: the online session this PC plays, so starting it again rejoins it "
             "(docs/port/NETPLAY.md 15). Deleted when the session ends.\nsession=%016llx\nrole=%s\nbuild=%s\n"
             "written=%lld\nwindow=%d\nrecover_at=%s\nrecovers=%d\n", (unsigned long long)id,
             S.role == PC_NET_HOST ? "host" : "guest", S.build, (long long)time(NULL), S.window_s, at, S.tk.recovers);
    if (pc_file_write_atomic(S.ticket, text, strlen(text)) != 0)
        fprintf(stderr, "rush2049pc: online: can't write %s (a rejoin after a crash won't find the session)\n",
                S.ticket);
    S.ticket_at = pc_real_ns();
}

static void ticket_drop(void)
{
    if (S.ticket[0] && pc_app_exists(S.ticket))
        remove(S.ticket);
}

/* A fresh ticket of this role and build: 0 with S.tk set; -1 none (or stale, or another build's). */
static int ticket_read(void)
{
    char text[1024] = "", v[64];
    FILE *f = S.ticket[0] ? fopen(S.ticket, "rb") : NULL;
    size_t n;
    if (!f)
        return -1;
    n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[n] = 0;
    if (pc_net_blob_value(text, "role", v, sizeof v) || strcmp(v, S.role == PC_NET_HOST ? "host" : "guest"))
        return -1;
    if (pc_net_blob_value(text, "build", v, sizeof v) || strcmp(v, S.build))
        return -1;
    memset(&S.tk, 0, sizeof S.tk);
    S.tk.recover_at = NET_NONE;
    if (!pc_net_blob_value(text, "session", v, sizeof v))
        S.tk.session = strtoull(v, NULL, 16);
    if (!pc_net_blob_value(text, "written", v, sizeof v))
        S.tk.written = atoll(v);
    if (!pc_net_blob_value(text, "recover_at", v, sizeof v) && strcmp(v, "none"))
        S.tk.recover_at = (uint32_t)strtoul(v, NULL, 10);
    if (!pc_net_blob_value(text, "recovers", v, sizeof v))
        S.tk.recovers = atoi(v);
    long long age = (long long)time(NULL) - S.tk.written;
    if (!S.tk.session || age < -60 || age > S.window_s + 60) {
        memset(&S.tk, 0, sizeof S.tk);
        S.tk.recover_at = NET_NONE;
        return -1;
    }
    return 0;
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
    char msg[256], text[1100];
    NetEnd e = net_end_reason(S.s, NULL, msg, sizeof msg);
    if (e == NET_END_NONE)
        snprintf(msg, sizeof msg, "this side left the session");
    if (S.gave_up)
        snprintf(text, sizeof text, "Nobody joined the online race in %d %s, so this PC stopped hosting. Host again "
                 "when your friend is ready to join.", S.gave_up % 60 ? S.gave_up : S.gave_up / 60,
                 S.gave_up % 60 ? "seconds" : "minutes");
    else if (S.role == PC_NET_GUEST && !S.joined) {
        char hint[400] = "";
        NetAddr host;
        if (e == NET_END_NO_ANSWER && net_peer_addr(S.s, &host))   /* an internet address: the host's router */
            net_nat_join_hint(&host, host.port, hint, sizeof hint);
        snprintf(text, sizeof text, "Couldn't join the online race at %s: %s.%s%s", S.join, msg, hint[0] ? " " : "",
                 hint);
    }
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
        NetRejoinInfo ri;
        net_rejoin_info(S.s, &ri);
        if (ri.phase == NET_RJ_TRANSFER && ri.rejoiner && now - said >= 2000000000u) {
            fprintf(stderr, "rush2049pc: online: getting the race so far from the %s: %llu of %llu KB\n", peer_name(),
                    (unsigned long long)(ri.bytes >> 10), (unsigned long long)(ri.total >> 10));
            said = now;
        } else if (now - said >= 10000000000u) {
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
    S.window_s = cfg->net_rejoin > 0 ? cfg->net_rejoin : 0;
    S.keep = cfg->net_keep;
    S.tk.recover_at = S.reported = NET_NONE;
    snprintf(S.build, sizeof S.build, "%s", build);
    if (S.window_s && o->data_dir && *o->data_dir) {   /* the rejoin ticket (netplay/rejoin-ROLE.txt) */
        char dir[PC_APP_PATH];
        pc_app_join(dir, sizeof dir, o->data_dir, "netplay");
        pc_app_mkdirs(dir);
        pc_app_join(S.ticket, sizeof S.ticket, dir, o->role == PC_NET_HOST ? "rejoin-host.txt" : "rejoin-guest.txt");
    }
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
    if (cfg->net_rumble) {   /* this player's controller holds a rumble pak (net.rumble), as the file says */
        c.files[0].data = (const uint8_t *)PC_NET_RUMBLE_FILE;
        c.files[0].size = strlen(PC_NET_RUMBLE_FILE);
        S.rumble[o->role == PC_NET_HOST ? 0 : 1] = 1;
        pak = NULL;
    }
    c.nfiles = 1;
    c.delay_fixed = cfg->net_delay;
    if (o->timeout_ms > 0)   /* --net-timeout: every silence, the rejoin window too */
        c.timeout_ms = c.window_ms = o->timeout_ms;
    else if (S.window_s)
        c.window_ms = S.window_s * 1000;
    if (ticket_read() == 0) {   /* this PC played a session lately that didn't end: rejoin it */
        c.rejoin_session = S.tk.session;
        fprintf(stderr, "rush2049pc: online: this PC played session %016llx until %lld s ago (%s): %s\n",
                (unsigned long long)S.tk.session, (long long)time(NULL) - S.tk.written, S.ticket,
                o->role == PC_NET_GUEST ? "asking the host to rejoin it" : "a guest that kept it has 3 s to knock");
    }
    c.recover = cfg->net_recover && S.tk.recovers < NET_RECOVER_MAX;
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
        pc_net_router_start(port, cfg->net_upnp, cfg->source);   /* net.upnp: the router opens the port (16) */
    } else {
        fprintf(stderr, "rush2049pc: online: joining %s as the guest (controller 2)%s\n", peer,
                c.join_code ? " with a join code" : "");
    }
    if (o->role == PC_NET_HOST && c.rejoin_session) {   /* its guest kept the session: it knocks every 0.25 s */
        uint64_t t0 = pc_real_ns();
        /* the router asked to open the port again: its guest's knocks may need it (12 s in all at most) */
        while ((pc_real_ns() - t0 < KNOCK_WAIT_NS || (pc_net_router_working() && pc_real_ns() - t0 < 4 *
                KNOCK_WAIT_NS)) && !net_rejoining(S.s) && net_state(S.s) != NET_ENDED && !pc_quit)
            net_os_sleep_ns(10000000u);
        if (net_rejoining(S.s)) {
            S.rejoin = 1;
            if (wait_state(NET_ACCEPT, "getting the session from the guest", 0) != 0)
                return failed(window);
            S.joined = 1;
            fprintf(stderr, "rush2049pc: online: rejoining session %016llx: the guest kept it, and sent its log\n",
                    (unsigned long long)c.rejoin_session);
            return 0;
        }
        net_rejoin_forget(S.s);
        ticket_drop();
        fprintf(stderr, "rush2049pc: online: no guest knocked for session %016llx: hosting a new session\n",
                (unsigned long long)c.rejoin_session);
    }
    if (o->role == PC_NET_HOST)
        return 0;   /* its guest is waited for with the window open (pc_net_wait_guest) */
    if (wait_state(NET_ACCEPT, "waiting for the host", 0) != 0)
        return failed(window);
    S.joined = 1;
    S.rejoin = net_rejoining(S.s);
    if (S.rejoin) {
        fprintf(stderr, "rush2049pc: online: rejoining session %016llx: the host kept it, and sent its log\n",
                (unsigned long long)c.rejoin_session);
    } else if (c.rejoin_session) {
        fprintf(stderr, "rush2049pc: online: the host doesn't have session %016llx any more: a new session\n",
                (unsigned long long)c.rejoin_session);
        ticket_drop();
        memset(&S.tk, 0, sizeof S.tk);
        S.tk.recover_at = NET_NONE;
    }
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
    if (S.rejoin)
        snprintf(title, sizeof title, "Rejoining the online race with %.62s.", peer);
    else if (S.role == PC_NET_HOST)
        snprintf(title, sizeof title, "A player joined from %.70s.", peer);
    else
        snprintf(title, sizeof title, "Joined the online race at %.65s.", peer);
    pc_menu_notice(title, S.rejoin ? "Catching up with the race when both PCs are ready...  Esc leaves."
                                   : "Starting when both PCs are ready...  Esc leaves.");
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

static int is_rumble(const void *data, size_t size)
{
    return size == strlen(PC_NET_RUMBLE_FILE) && !memcmp(data, PC_NET_RUMBLE_FILE, size);
}

int pc_net_paks(int window)
{
    const void *data[2];
    size_t size[2];
    int mine = S.role == PC_NET_HOST ? 0 : 1;
    char why[200], text[400];
    if (S.rejoin) {   /* both paks as the session had them at its start, from the side that kept it */
        for (int k = 0; k < 2; k++) {
            free(S.pak[k]);
            S.pak[k] = NULL;
            S.rumble[k] = 0;
            if (!net_session_files(S.s, k) || !net_session_file(S.s, k, 0, &data[k], &size[k]) || !size[k])
                continue;   /* that player has no pak */
            if ((S.rumble[k] = is_rumble(data[k], size[k])) != 0)
                continue;   /* a rumble pak (net.rumble) */
            if (size[k] != PC_PAK_SIZE || pc_pak_image_check(data[k], size[k], why, sizeof why) != PC_PAK_IMAGE_OK) {
                snprintf(text, sizeof text, "the session's controller pak %d can't be used: %s", k + 1, why);
                return pc_net_reject(text, "a controller pak of the session is damaged", window);
            }
            if (!(S.pak[k] = malloc(PC_PAK_SIZE)))
                return pc_net_reject("out of memory", NULL, window);
            memcpy(S.pak[k], data[k], PC_PAK_SIZE);
        }
        return 0;
    }
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
        if (is_rumble(data[k], size[k])) {
            S.rumble[k] = 1;
            fprintf(stderr, "rush2049pc: online: the %s's player has a rumble pak (net.rumble: no controller pak)\n",
                    peer_name());
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
        if (ch < 2 && S.rumble[ch]) {   /* net.rumble: the same on both PCs, from the handshake's files */
            pc_si_set_pak_image(ch, NULL);
            pc_si_set_pak(ch, PC_PAK_RUMBLE);
            cfg->pak[ch] = PC_CFG_PAK_RUMBLE;
            continue;
        }
        pc_si_set_pak_image(ch, ch < 2 ? S.pak[ch] : NULL);
        cfg->pak[ch] = ch < 2 && S.pak[ch] ? PC_CFG_PAK_MEMORY : PC_CFG_PAK_NONE;
    }
    fprintf(stderr, "rush2049pc: online: controller 1 (the host's player) %s, controller 2 (the guest's) %s; in "
            "memory only: nothing is written to the disk during the session\n",
            S.rumble[0] ? "has a rumble pak" : S.pak[0] ? "has the host's controller pak" : "has no controller pak",
            S.rumble[1] ? "has a rumble pak" : S.pak[1] ? "has the guest's controller pak" : "has no controller pak");
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
    if (e == NET_END_DESYNC || e == NET_END_RECOVER) {
        if (detail == 0 && e == NET_END_DESYNC)
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

/* The quit key's words for a notice: " Esc ends the session." */
static void quit_words(char *out, size_t n, const char *what)
{
    if (S.quit_key[0])
        snprintf(out, n, " %s %s.", S.quit_key, what);
    else
        snprintf(out, n, " Closing the window %s.", what);
}

static void mmss(char *out, size_t n, uint64_t ms)
{
    snprintf(out, n, "%u:%02u", (unsigned)(ms / 60000), (unsigned)(ms / 1000 % 60));
}

/* The replay's notice (the rejoiner's screen, nothing else drawn meanwhile) and its line in the log. */
static void catchup_notice(uint32_t n, uint64_t now)
{
    char title[96], line[160], q[64], left[16] = "";
    double t = (now - S.catch_t0) / 1e9, rate = t > 0 ? n / t : 0;
    unsigned pct = S.live ? (unsigned)((uint64_t)n * 100 / S.live) : 100;
    if (rate > 0 && n > 600)
        mmss(left, sizeof left, (uint64_t)((S.live - n) / rate * 1000));
    quit_words(q, sizeof q, "leaves");
    snprintf(title, sizeof title, "Catching up with the race: %u%%", pct);
    snprintf(line, sizeof line, "Retrace %u of %u%s%s.%s", n, S.live, left[0] ? ", about " : "", left, q);
    pc_menu_notice(title, line);
    pc_gfx_redraw();
    S.notice = 1;
    S.notice_at = now;
    if (now - S.catch_said >= CATCHUP_SAY_NS) {
        fprintf(stderr, "rush2049pc: online: catching up: retrace %u of %u (%u%%), %.0f a second\n", n, S.live, pct,
                rate);
        S.catch_said = now;
    }
}

/* The replay reached the live retrace: the mode before it, drawing and the sound back; the session's lockstep from
 * here (net_submit). */
static void catchup_done(uint32_t n)
{
    uint64_t now = pc_real_ns();
    double t = (now - S.catch_t0) / 1e9;
    pc_catchup_set(0);
    S.catchup = 0;
    if (S.notice)
        pc_menu_notice(NULL, NULL), S.notice = 0;
    fprintf(stderr, "rush2049pc: online: caught up: replayed %u retraces (%.1f s of the race) in %.2f s, %.0f a second, "
            "%.1f times real time, every retrace's hash the %s's; live from retrace %u%s\n", n, n / 60.0, t,
            t > 0 ? n / t : 0.0, t > 0 ? n / 60.0 / t : 0.0, peer_name(), n,
            S.tk.recover_at != NET_NONE ? " (the desync is recovered from)" : "");
    if (S.tk.recover_at != NET_NONE)
        fprintf(stderr, "rush2049pc: online: recovered from the desync at retrace %u: the rebuilt game had the host's "
                "hash at every retrace past it\n", S.tk.recover_at);
    S.tk.recover_at = NET_NONE;
    ticket_write();
}

static void src_local(void *user, uint32_t n, const PcPad local[4])
{
    (void)user;
    NetPad p = {local[0].buttons, local[0].stick_x, local[0].stick_y};
    if (S.catchup) {   /* the replay: the log's pads, not this PC's */
        if (n < S.live)
            return;
        catchup_done(n);
    }
    if (net_frozen(S.s)) {
        Q.held = 1, Q.n = n, Q.pad = p;
        return;
    }
    net_submit(S.s, n, &p);   /* -1 once the session has ended: src_pads says why */
}

/* The other side is gone or rejoining: what this player is waiting for, and for how long it may. */
static void waiting_notice(int ms, uint64_t now)
{
    char title[96], line[192], q[64], a[16], b[16];
    NetRejoinInfo ri;
    net_rejoin_info(S.s, &ri);
    quit_words(q, sizeof q, "ends the session");
    if (ri.phase == NET_RJ_RECOVER && ri.recover_at != S.reported) {   /* the desync's report, once */
        S.reported = ri.recover_at;
        fprintf(stderr, "rush2049pc: online: the games went out of sync at retrace %u: the guest rebuilds its game by "
                "replay of this PC's log (net.recover)\n", ri.recover_at);
        if (pc_hash_report(S.report_path, ri.recover_at) == 0)
            fprintf(stderr, "rush2049pc: online: the desync report (retrace %u's state as hashed): %s\n",
                    ri.recover_at, S.report_path);
    }
    switch (ri.phase) {
    case NET_RJ_SILENT:
        mmss(a, sizeof a, ri.silent_ms);
        mmss(b, sizeof b, ri.window_ms > ri.silent_ms ? ri.window_ms - ri.silent_ms : 0);
        snprintf(title, sizeof title, "Waiting for the %s to come back...", peer_name());
        if (ri.closed)
            snprintf(line, sizeof line, "Its game closed %s ago; waiting %s more for it to start again.%s", a, b, q);
        else
            snprintf(line, sizeof line, "Nothing from it for %s; waiting %s more.%s", a, b, q);
        break;
    case NET_RJ_RECOVER:
        mmss(a, sizeof a, ri.window_ms > ri.silent_ms ? ri.window_ms - ri.silent_ms : 0);
        snprintf(title, sizeof title, "The games went out of sync at retrace %u.", ri.recover_at);
        snprintf(line, sizeof line, "The guest's game is starting again to rebuild the race (up to %s).%s", a, q);
        break;
    case NET_RJ_TRANSFER:
        snprintf(title, sizeof title, "The %s is rejoining...", peer_name());
        snprintf(line, sizeof line, "Sending it the race so far: %llu of %llu KB.%s",
                 (unsigned long long)(ri.bytes >> 10), (unsigned long long)(ri.total >> 10), q);
        break;
    case NET_RJ_CATCHUP:
        snprintf(title, sizeof title, "The %s is catching up: %u%%", peer_name(),
                 ri.live ? (unsigned)((uint64_t)ri.at * 100 / ri.live) : 100);
        snprintf(line, sizeof line, "It replays the race: retrace %u of %u.%s", ri.at, ri.live, q);
        break;
    default:
        snprintf(title, sizeof title, "Waiting for the %s...", peer_name());
        snprintf(line, sizeof line, "%.1f s.%s", ms / 1000.0, q);
        break;
    }
    pc_menu_notice(title, line);
    pc_gfx_redraw();
    S.notice = 1;
    S.notice_at = now;
    if (ri.phase != NET_RJ_NONE && now - S.catch_said >= CATCHUP_SAY_NS) {   /* the log has what the screen says */
        fprintf(stderr, "rush2049pc: online: %s %s\n", title, line);
        S.catch_said = now;
    }
}

static int src_pads(void *user, uint32_t n, PcPad out[4])
{
    (void)user;
    NetPad p[NET_MAX_PORTS];
    if (S.catchup && n < S.live) {   /* the replay: the other side's log */
        uint64_t now = pc_real_ns();
        if (net_state(S.s) == NET_ENDED || !net_catchup_pads(S.s, n, p)) {
            if (!S.ended && !pc_quit)
                pc_quit = session_ended() == NET_END_PEER_LEFT ? 1 : 1 + EXIT_SESSION;
            pc_pads_signal();
            return PC_PADS_WAIT;
        }
        for (int i = 0; i < 4; i++)
            out[i] = (PcPad){p[i].buttons, p[i].stick_x, p[i].stick_y, i < 2, 0};
        if ((n & 63) == 0)
            net_catchup_at(S.s, n);
        if (!S.notice || now - S.notice_at >= NOTICE_EVERY_NS)
            catchup_notice(n, now);
        return PC_PADS_READY;
    }
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
        if (S.window_s && pc_real_ns() - S.ticket_at >= TICKET_EVERY_NS)
            ticket_write();   /* the ticket says the session is still played */
        return PC_PADS_READY;
    }
    if (r < 0) {   /* the main loop stops: 0 when the other player left or this side rebuilds, else EXIT_SESSION */
        if (!S.ended && !pc_quit) {
            NetEnd e = session_ended();
            pc_quit = e == NET_END_PEER_LEFT || e == NET_END_RECOVER ? 1 : 1 + EXIT_SESSION;
        }
        pc_pads_signal();
        return PC_PADS_WAIT;
    }
    int ms = net_waiting_ms(S.s);
    uint64_t now = pc_real_ns();
    if (ms >= WAITING_MS && (!S.notice || now - S.notice_at >= NOTICE_EVERY_NS))
        waiting_notice(ms, now);
    pc_pads_signal();   /* the gate asks again at once: the waiting is net_pads', woken by the net thread */
    return PC_PADS_WAIT;
}

static void src_hash(void *user, uint32_t n, const PcStateHash *h)
{
    (void)user;
    if (S.ended || net_state(S.s) == NET_ENDED)
        return;
    if (S.catchup && n < S.live) {   /* the replay: each retrace's hash against the other side's log */
        uint32_t folded = (uint32_t)(h->state ^ h->state >> 32);
        int ok = net_catchup_hash(S.s, n, folded) != 0;
        if (ok && (n == 0 || h->full))
            ok = net_catchup_window_hash(S.s, n, (h->full ? h->full_hash : 0) ^ (n == 0 ? S.setup_hash : 0)) != 0;
        if (!ok && !pc_quit) {   /* the session ended: the rebuilt game differs (a desync) */
            session_ended();
            pc_quit = 1 + EXIT_SESSION;
        }
        return;
    }
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
    char name[128];
    for (int k = 1; k < 100; k++) {
        char probe[PC_APP_PATH + 16];
        snprintf(name, sizeof name, k == 1 ? "%s-%s" : "%s-%s-%d", stamp, S.role == PC_NET_HOST ? "host" : "guest", k);
        pc_app_join(S.base, sizeof S.base, dir, name);
        snprintf(probe, sizeof probe, "%s.pads", S.base);
        if (!pc_app_exists(probe))
            break;
    }
    snprintf(S.pads_path, sizeof S.pads_path, "%s.pads", S.base);
    snprintf(S.report_path, sizeof S.report_path, "%s.desync.txt", S.base);
    {   /* net.keep: the older sessions beyond the newest (this one counted) removed (netkeep.c) */
        char say[200];
        int kept, gone = pc_net_keep(dir, name, S.keep, (long long)now, &kept, say, sizeof say);
        if (gone)
            fprintf(stderr, "rush2049pc: online: netplay/: %d older session%s removed (%s%s: each one's input log, "
                    "paks and report), %d kept besides this one (net.keep = %d; replays/ is never touched)\n", gone,
                    gone == 1 ? "" : "s", say, gone > 3 ? ", ..." : "", kept, S.keep);
    }
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
    snprintf(S.header, n + 512, "%s# online session: the %s's log (the other PC keeps its own)\n# paks: %s\n%s",
             must_match, S.role == PC_NET_HOST ? "host" : "guest", S.base,
             S.rejoin ? (S.tk.recover_at != NET_NONE ? "# a rejoin after a desync: the retraces before it replayed "
                                                       "from the host's log\n"
                                                     : "# a rejoin: the retraces before it replayed from the other "
                                                       "PC's log\n")
                      : "");
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
    pc_input_session_motor(S.role == PC_NET_HOST ? 0 : 1);   /* this player's motor alone, to this PC's controller */
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
    if (S.rejoin) {   /* the other side's START: then the replay of its log, from the boot */
        net_accept(S.s);
        r = wait_state(NET_STARTING, S.role == PC_NET_HOST ? "waiting for the guest to start the rejoin"
                                                           : "waiting for the host to start the rejoin", 1);
        if (r == PC_NET_CANCELLED)
            return r;
        if (r != 0 || !net_catchup(S.s, &S.live))
            return failed(window);
        S.catchup = 1;
        S.catch_t0 = S.catch_said = pc_real_ns();
        pc_catchup_set(1);
        fprintf(stderr, "rush2049pc: online: rejoining session %016llx: replaying its %u retraces (%.1f s of the race) "
                "unpaced, nothing drawn or played, each retrace's hash checked against the %s's; then live\n",
                (unsigned long long)net_session_id(S.s), S.live, S.live / 60.0, peer_name());
        S.started = 1;
        ticket_write();
        return 0;
    }
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
    ticket_write();
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
#ifdef SIGTERM
    int term = pc_quit_signal == SIGTERM;
#else
    int term = 0;
#endif
    if (e == NET_END_NONE && S.started && S.window_s && net_session_id(S.s) &&
        (pc_gfx_window_closed() || term || pc_quit == 1 + 129)) {
        /* the window closed, or the system ended the program (a shutdown, the terminal gone): the game went, not
         * the player: the other PC waits for it to come back, and starting it again the same way rejoins */
        net_end_text(S.s, NET_END_GONE, pc_gfx_window_closed() ? "the window closed" : "the program was ended");
        e = net_end_reason(S.s, &detail, msg, sizeof msg);
        fprintf(stderr, "rush2049pc: online: %s: the %s waits up to %d:%02d for this PC; starting the game again the "
                "same way (Host or Join online race) rejoins the race\n", pc_gfx_window_closed() ? "the window closed"
                : "the program was ended", peer_name(), S.window_s / 60, S.window_s % 60);
    } else if (e == NET_END_NONE) {   /* this side stopped: a limit, the quit key, the overlay's Quit, Ctrl-C */
        if (S.started && !net_linger(S.s, LINGER_MS))
            fprintf(stderr, "rush2049pc: online: the %s may not have this side's last inputs\n", peer_name());
        net_end(S.s, NET_END_LOCAL);
        e = net_end_reason(S.s, &detail, msg, sizeof msg);
    }
    S.end_local = e == NET_END_LOCAL;
    if (e != NET_END_LOCAL)
        session_ended();   /* while the main loop stopped for its own reason: the report all the same */
    if (S.catchup) {   /* it ended in the replay: the mode back for the rest of the run */
        pc_catchup_set(0);
        S.catchup = 0;
    }
    if (e == NET_END_RECOVER) {   /* this PC's game rebuilds: the program starts again and rejoins */
        S.tk.recover_at = detail;
        S.tk.recovers++;
        ticket_write();
        if (getenv("RUSH_SIM_DESYNC_ONCE") && *getenv("RUSH_SIM_DESYNC_ONCE"))   /* tests: the desync was a one-off */
            unsetenv("RUSH_SIM_DESYNC");
        S.reexec = S.ticket[0] != 0;
        fprintf(stderr, "rush2049pc: online: %s\n", S.reexec ? "rebuilding: the program starts again, rejoins the "
                "session and replays the host's log past the desync" : "can't rebuild without a rejoin ticket "
                "(net.rejoin = off, or no data folder)");
        if (!S.reexec && code == 0)
            code = EXIT_SESSION;
    } else if (e != NET_END_GONE) {
        ticket_drop();   /* it ended (a player left, a timeout, a desync): nothing to rejoin */
    }
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
    if (st.rejoins || st.recoveries)
        fprintf(stderr, "rush2049pc: online: %u rejoin%s, %u desync%s recovered from\n", st.rejoins,
                st.rejoins == 1 ? "" : "s", st.recoveries, st.recoveries == 1 ? "" : "s");
    if ((e == NET_END_RECOVER && S.reexec) || e == NET_END_GONE) {
        /* no box: the program comes back at once, or its window is closing */
    } else if (e != NET_END_LOCAL && e != NET_END_PEER_LEFT) {
        snprintf(text, sizeof text, "The online session ended (%s): %s.%s", net_end_name(e), msg,
                 e == NET_END_DESYNC ? "\n\nThe log says where the input log and the desync report are." : "");
        show(text, window);
        if (code == 0)
            code = EXIT_SESSION;
    } else if (e == NET_END_PEER_LEFT) {
        snprintf(text, sizeof text, "The %s left the online session.", peer_name());
        show(text, window);
    }
    pc_net_router_finish(e == NET_END_GONE);   /* the mapping removed; this PC's game coming back: its lease */
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
