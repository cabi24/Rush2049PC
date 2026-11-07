/* port/net/net.h: online play's network library: a lockstep session between two PCs over UDP
 * (docs/port/NETWORK.md; the plan is docs/port/NETPLAY.md). Plain C types only, no game headers.
 *
 * What a session does:
 *   - connects a guest to a host (one UDP socket and port each; IPv4 and IPv6, host names), checks the build,
 *     sends the host's settings blob and each side's files (pak images) reliably, and agrees a start time;
 *   - at every retrace N takes this side's pads (NetPad, one per port it owns), assigns them to retrace N + D
 *     (D: this side's own input delay, adaptive or fixed), sends every input the peer hasn't acknowledged, and
 *     gives back the pads of all ports for N once the peer's are known;
 *   - compares the state hash both sides give for each retrace, and ends on the first mismatch (a desync);
 *   - gives the caller's paced clock a small adjustment each retrace so both sides reach a retrace at the same
 *     wall time (time sync);
 *   - ends on BYE, a silence (15 s while connecting; the rejoin window, window_ms, once running), a protocol error or
 *     a desync, with the reason;
 *   - keeps the session's log (every retrace's pads, its own hash of every retrace, the window hashes) and the
 *     handshake's settings and files, so a side whose game is gone (a crash, a closed window, a reboot, or a desync
 *     it rebuilds from: recover) can rejoin: a new process rejoins the session by its id (rejoin_session), gets the
 *     log from the side that kept it, replays it unpaced checking every retrace's hash, and goes on live from where
 *     the keeper waits (docs/port/NETWORK.md 12, NETPLAY.md 15).
 *
 * Threads. A session is one object behind one lock, and every function here may be called from any thread.
 * There are two ways to drive it:
 *   - net_start_thread: a service thread of the session's own pumps the socket and the timers (acks, resends,
 *     keepalives, pings, timeouts). The caller only submits, asks for pads (net_pads waits on a condition the
 *     service thread signals), and gives hashes. This is the mode for the game: the network keeps being served
 *     while the main loop waits on the N64 threads or sleeps.
 *   - without it, the caller pumps: net_poll (non-blocking) and net_wait (block until a packet or a timer is
 *     due). net_pads with a timeout pumps by itself. The deterministic tests drive two sessions this way on a
 *     virtual clock and an in-memory transport (NetTransport).
 *
 * The retrace loop the game will run (one owned port shown):
 *
 *     net_submit(s, n, &my_pad);                         // the pads polled at retrace n
 *     while ((r = net_pads(s, n, pads, 10)) == 0)        // all four ports' pads of retrace n
 *         pump_window_events(), show_waiting_if(net_waiting_ms(s) >= 250);
 *     if (r < 0) end_session(net_end_reason(s, ...));
 *     ... run retrace n with pads ...
 *     net_hash(s, n, state_hash(n));                     // and net_window_hash every 60
 *     clock_adjust(net_pace_adjust_ns(s));               // add to the interval before retrace n + 1
 */
#ifndef NET_H
#define NET_H

#include <stddef.h>
#include <stdint.h>

#include "net_sock.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NET_MAX_PORTS 4
#define NET_DEFAULT_PORT 24049
#define NET_FILES_MAX 8                 /* = NET_MAX_FILES */
#define NET_FILE_MAX_SIZE (1u << 20)    /* each file, at most */
#define NET_LOG_MAX_SIZE (64u << 20)    /* a rejoin's log, at most (about 39 hours of two players) */
#define NET_RECOVER_MAX 3               /* desyncs a session recovers from; past it a desync ends it */
#define NET_SETTINGS_MAX_SIZE (64u << 10)

/* A controller's state as the N64 reads it (PcPad): 4 bytes on the wire, u16 buttons, s8 x, s8 y. */
typedef struct NetPad {
    uint16_t buttons;
    int8_t stick_x, stick_y;
} NetPad;

typedef enum NetRole { NET_HOST = 0, NET_GUEST = 1 } NetRole;

typedef enum NetState {
    NET_CONNECTING = 0,   /* host: waiting for a guest; guest: sending HELLO */
    NET_HANDSHAKE,        /* settings and files on their way */
    NET_ACCEPT,           /* everything received: the caller checks it, then net_accept */
    NET_READY,            /* accepted, waiting for the other side and the start */
    NET_STARTING,         /* the start time is known; retrace 0 hasn't been submitted */
    NET_RUNNING,
    NET_ENDED
} NetState;

typedef enum NetEnd {
    NET_END_NONE = 0,
    NET_END_LOCAL,        /* this side called net_end */
    NET_END_PEER_LEFT,    /* the peer sent BYE */
    NET_END_TIMEOUT,      /* nothing from the peer for timeout_ms */
    NET_END_NO_ANSWER,    /* guest: no HELLO_ACK within timeout_ms */
    NET_END_PROTOCOL,     /* the peer broke the protocol (two values for one input, a bad file) */
    NET_END_DESYNC,       /* the first retrace whose hashes differ is the detail */
    NET_END_VERSION,      /* different builds or protocol versions */
    NET_END_MISMATCH,     /* the agreement hashes differ (READY/START) */
    NET_END_REJECTED,     /* the caller (or the peer's) rejected the settings or files */
    NET_END_BUSY,         /* the host has a guest already */
    NET_END_SOCKET,       /* the socket failed */
    NET_END_RECOVER,      /* the guest's game went out of sync (detail: the retrace): it rebuilds by replay and rejoins */
    NET_END_GONE,         /* this side's game closed (its window, the system ending it), not the player leaving: the
                             peer waits the rejoin window for it to come back (BYE gone) */
    NET_END_COUNT
} NetEnd;

/* How packets travel: UDP by default; tests give an in-memory one. send returns 0 (or -1 for a bad address);
 * recv returns a datagram's length, 0 when none waits, -2 for a datagram longer than cap (dropped), -1 an error;
 * wait blocks until something may be readable or timeout_us passes (NULL: the session sleeps instead). */
typedef struct NetTransport {
    void *ctx;
    int (*send)(void *ctx, const NetAddr *to, const void *buf, int len);
    int (*recv)(void *ctx, NetAddr *from, void *buf, int cap);
    int (*wait)(void *ctx, int timeout_us);
} NetTransport;

typedef struct NetFile {
    const void *data;
    size_t size;
} NetFile;

typedef struct NetConfig {
    NetRole role;
    const char *peer;          /* guest: "host", "host:port", "[v6]:port" (port NET_DEFAULT_PORT when none) */
    const char *bind;          /* the local address to bind ("" or NULL: any, dual-stack) */
    int port;                  /* host: the port to listen on (0: any, net_local_port says which);
                                  guest: its local port (0: any) */
    uint8_t ports;             /* the controller ports this side owns, bit i = port i + 1 (host 0x1, guest 0x2) */
    const char *build;         /* this program's build (the commit): must be the same on both sides */
    const void *settings;      /* host: the canonical settings blob the guest adopts; guest: its own, compared */
    size_t settings_size;
    NetFile files[NET_FILES_MAX];   /* the files this side sends (its pak images), in order */
    int nfiles;
    int auto_accept;           /* skip NET_ACCEPT: accept what arrives (tests, nothing to check) */
    int delay_fixed;           /* > 0: this side's input delay is fixed (--net-delay N) */
    int delay_min, delay_cap;  /* the adaptive delay's range, retraces (default 1, 30) */
    double delay_margin_ms;    /* what the delay covers beyond the slowest input's transit (default 3) */
    int64_t retrace_ns;        /* the nominal retrace (default 1e9 / 60) */
    int timeout_ms;            /* silence that ends a session before it runs (default 15000); the guest's wait for an
                                  answer */
    int window_ms;             /* silence that ends it once it runs: the rejoin window (0: timeout_ms) */
    uint64_t rejoin_session;   /* nonzero: this side rejoins that session (net_session_id's), rebuilding it from the
                                  peer's log: a guest's HELLO asks for it; a host waits for its guest's knock (and
                                  takes a new guest instead until net_rejoin_forget) */
    int recover;               /* a desync: the host waits for the guest to rebuild by replay (NET_END_RECOVER there,
                                  BYE recover), up to NET_RECOVER_MAX times; 0: a desync ends the session */
    int wait_report_ms;        /* net_pads waiting this long is logged as "waiting for the peer" (default 250) */
    const char *join_code;     /* NULL or "": no tags; else every packet carries a BLAKE2s tag keyed by it */
    const char *sim;           /* a RUSH_SIM_NET model for this session; NULL: getenv("RUSH_SIM_NET") */
    uint64_t seed;             /* the simulator's seed (0: from the role) */
    uint64_t (*now_ns)(void *user);           /* the clock (NULL: monotonic, net_os_now_ns) */
    void (*log)(void *user, const char *line); /* NULL: stderr, "net: ..." */
    void *user;
    const NetTransport *transport;   /* NULL: a UDP socket */
    NetAddr peer_addr;         /* with a transport: the guest's peer (instead of peer) */
    int quiet;                 /* log only ends and errors */
} NetConfig;

typedef struct NetStats {
    NetState state;
    NetEnd end;
    uint32_t end_detail;
    uint32_t retrace;           /* the last retrace submitted (NET_NONE before 0) */
    uint32_t peer_retrace;      /* the peer's, from its last packet */
    /* round trip, from every packet's timestamps (NTP's arithmetic) */
    double rtt_ms, rtt_min_ms, rtt_max_ms, jitter_ms, clock_offset_ms;
    int rtt_samples;
    /* the input delay */
    int delay, delay_min_seen, delay_max_seen, delay_changes, peer_delay;
    double transit_ms;          /* the slowest of the peer's inputs to arrive lately (3 s window) */
    double feedback_ms;         /* the peer's report of this side's: what this side's delay follows */
    uint64_t samples_repeated, samples_dropped;   /* by delay changes */
    /* waiting for the peer's input */
    uint64_t stalls;
    double stall_ms_total, stall_ms_max;
    /* time sync */
    uint64_t sync_corrections;
    double sync_ms_total, advantage_ms;
    /* packets */
    uint64_t sent, received, bytes_sent, bytes_received, lost, duplicates, reordered;
    uint64_t resends, keepalives;
    uint64_t rejected;          /* malformed, wrong session, wrong source, bad tag, out of window */
    uint64_t rejected_source, rejected_tag, hellos_limited;
    /* hashes */
    uint64_t hash_checks, window_hash_checks;
    /* the simulator */
    uint64_t sim_dropped, sim_duplicated, sim_reordered;
    /* rejoins: the peer's (this side kept the session) or this side's own, desyncs recovered from, outages (inputs
     * whose transit was past the delay's cap: not latency, so not followed by the delay) */
    uint32_t rejoins, recoveries;
    uint64_t outage_inputs;
} NetStats;

#define NET_NONE 0xFFFFFFFFu

typedef struct NetSession NetSession;

void net_config_defaults(NetConfig *c);
/* Opens the socket (or takes the transport), resolves the peer, starts the handshake. NULL with err on failure. */
NetSession *net_create(const NetConfig *c, char *err, size_t errlen);
/* Ends the session (BYE when it was running), stops the service thread, closes the socket. */
void net_destroy(NetSession *s);

/* The service thread (see the top). 0 on success. */
int net_start_thread(NetSession *s);
/* Pump once without blocking: receive, handle, resend, keep alive, time out. */
void net_poll(NetSession *s);
/* Block until a datagram may be waiting or a timer is due, up to timeout_us; then pump. */
void net_wait(NetSession *s, int timeout_us);

NetState net_state(NetSession *s);
int net_local_port(NetSession *s);
/* The peer's address: the host's (the guest, from the start), the guest's (the host, once one has joined). 1, or 0
 * while there is none. */
int net_peer_addr(NetSession *s, NetAddr *out);
const char *net_state_name(NetState st);
const char *net_end_name(NetEnd e);

/* The handshake. The guest's copy of the host's settings blob (1 when it has arrived), and a received file. */
int net_peer_settings(NetSession *s, const void **data, size_t *size);
int net_settings_differ(NetSession *s);   /* guest: its own blob was not the host's (the host's rules) */
int net_peer_files(NetSession *s);
int net_peer_file(NetSession *s, int index, const void **data, size_t *size);
/* The caller has checked and adopted what arrived: READY goes out (called before NET_ACCEPT: as soon as it has). */
void net_accept(NetSession *s);
/* Retrace 0's wall time on this side's clock, once known (STARTING): 1, else 0. */
int net_start_time(NetSession *s, uint64_t *local_ns);

/* At retrace n (n = 0, 1, 2, ... in order): this side's pads, one per owned port in port order. The library
 * assigns them to retrace n + D: when D grew since the last retrace, the retraces skipped repeat the last input
 * assigned; when it shrank, this sample is dropped (its retrace already has one). 0, or -1 (ended, or out of
 * order). */
int net_submit(NetSession *s, uint32_t n, const NetPad *pads);
/* All ports' pads of retrace n (ports nobody owns are zero): 1 when known, 0 not yet within timeout_ms, -1 the
 * session ended. After the peer's BYE (NET_END_PEER_LEFT) the retraces it ran itself still give 1 when their pads
 * are known here, so this side reaches the retrace the peer left at. */
int net_pads(NetSession *s, uint32_t n, NetPad out[NET_MAX_PORTS], int timeout_ms);
/* How long the current net_pads retrace has been waiting for the peer, ms (0: not waiting). */
int net_waiting_ms(NetSession *s);
/* The state hash of retrace n, after running it (n in order). */
void net_hash(NetSession *s, uint32_t n, uint32_t hash);
/* An extra hash of retrace n (a whole window hash, every 60 retraces or so). */
void net_window_hash(NetSession *s, uint32_t n, uint64_t hash);
/* Time sync: ns to add to the interval before the next retrace, 0 .. 1 ms (positive: this side is ahead and waits;
 * the side behind never races). Call once per retrace, after net_pads gave this retrace's pads. The caller's clock:
 *   deadline(n + 1) = deadline(n) + retrace_ns + adjust, or, when net_pads(n) gave the pads more than half a retrace
 *   after deadline(n), the moment it gave them + retrace_ns + adjust (re-based after a stall: never catching up;
 *   a smaller lateness stays on the grid, and time sync evens out what is left);
 *   deadline(0) = net_start_time, on the session's clock (cfg.now_ns; give the game's own). */
int64_t net_pace_adjust_ns(NetSession *s);
/* This side's input delay now, retraces. */
int net_delay(NetSession *s);
/* The simulator's pause (tests): this peer should stand still, as a stopped process would. */
int net_frozen(NetSession *s);

/* End the session: BYE (with reason) to the peer. net_end_text: with this sentence for the peer and the log (the
 * peer gets its first 63 characters): why the settings were rejected ("the host's ROM is ..."). */
void net_end(NetSession *s, NetEnd reason);
void net_end_text(NetSession *s, NetEnd reason, const char *text);
/* Before this side leaves at a retrace of its own: wait until the peer has every input this side assigned (its
 * acknowledgements), up to timeout_ms, so that after the BYE it can still run the retraces this side ran (net_pads
 * gives them after a BYE). 1 when the peer has them all. */
int net_linger(NetSession *s, int timeout_ms);
/* Why it ended (NET_END_NONE: it hasn't), its detail (the desync's retrace), and a sentence for the log. */
NetEnd net_end_reason(NetSession *s, uint32_t *detail, char *msg, size_t n);
void net_stats(NetSession *s, NetStats *st);
/* One line of the stats for a log. */
void net_stats_line(const NetStats *st, char *buf, size_t n);

/* ---------------------------------------------------------------- rejoining (docs/port/NETWORK.md 12)
 *
 * The keeper (the side whose game goes on) waits at the first retrace it lacks the peer's input for; the rejoiner's
 * HELLO (or the keeper's knock, when the host is the one that rejoins) sets the retrace the session goes on from,
 * live: everything the keeper ran before it is the log. The rejoiner gets the session's settings, both sides' files
 * as they were at its start and the log, the caller checks and adopts them (NET_ACCEPT, net_accept), and at START
 * replays retraces 0 .. live-1 from the log as fast as it can (net_catchup_*), checking its state hash at every
 * retrace against the keeper's; its first net_submit, of retrace live, puts it back in the lockstep. */

/* The session's id: the same in both sides' sessions, and in a rejoin's (0 before the start). */
uint64_t net_session_id(NetSession *s);

typedef enum NetRejoinPhase {
    NET_RJ_NONE = 0,
    NET_RJ_SILENT,     /* the peer is silent: waiting for it to come back (its network, or a rejoin), window_ms */
    NET_RJ_RECOVER,    /* a desync: waiting for the guest to rebuild and rejoin (recover_at) */
    NET_RJ_TRANSFER,   /* the log on its way (bytes of total) */
    NET_RJ_CATCHUP,    /* the rejoiner replays the log (at of live) */
} NetRejoinPhase;

typedef struct NetRejoinInfo {
    NetRejoinPhase phase;
    int rejoiner;             /* 1: this side rebuilds; 0: it keeps the session */
    int closed;               /* NET_RJ_SILENT: the peer said its game closed (BYE gone), rather than went quiet */
    uint32_t silent_ms;       /* since the peer's last packet */
    uint32_t window_ms;       /* the rejoin window */
    uint64_t bytes, total;    /* NET_RJ_TRANSFER */
    uint32_t at, live;        /* NET_RJ_CATCHUP: retraces replayed of those to replay */
    uint32_t recover_at;      /* the desync being recovered from, NET_NONE */
    uint32_t rejoins, recoveries;
} NetRejoinInfo;
void net_rejoin_info(NetSession *s, NetRejoinInfo *out);
/* This side rejoins (its HELLO, or the knock it answered, was taken as a rejoin): 1; 0 a session of its own. */
int net_rejoining(NetSession *s);
/* A host with rejoin_session that no knock came for: it goes on as a host of a new session. */
void net_rejoin_forget(NetSession *s);
/* The session's settings blob (the host's) and the files as they were at its start, side 0 the host's and 1 the
 * guest's: from the handshake, or for a rejoiner from the keeper (once they have arrived: NET_ACCEPT). 1, or 0. */
int net_session_settings(NetSession *s, const void **data, size_t *size);
int net_session_files(NetSession *s, int side);
int net_session_file(NetSession *s, int side, int index, const void **data, size_t *size);
/* The rejoiner, from START until its first net_submit: 1 and *live (the retraces to replay first); 0 otherwise. */
int net_catchup(NetSession *s, uint32_t *live);
/* Retrace n's pads from the keeper's log (n < live): 1, or 0. */
int net_catchup_pads(NetSession *s, uint32_t n, NetPad out[NET_MAX_PORTS]);
/* This side's state hash of replayed retrace n against the keeper's: 1 the same; 0 it differs, and the session ends
 * (a desync at n; at the retrace a recovery is from, said to be deterministic); -1 the log has none. The window
 * hash likewise (1 also when the keeper took none at n). */
int net_catchup_hash(NetSession *s, uint32_t n, uint32_t hash);
int net_catchup_window_hash(NetSession *s, uint32_t n, uint64_t hash);
/* The replay is at retrace n: the keeper hears of it (PROGRESS, 10 times a second). */
void net_catchup_at(NetSession *s, uint32_t n);

/* Tests: handle buf as a datagram that just arrived from `from` (the fuzzers). */
void net_test_inject(NetSession *s, const void *buf, int len, const NetAddr *from);

#ifdef __cplusplus
}
#endif

#endif
