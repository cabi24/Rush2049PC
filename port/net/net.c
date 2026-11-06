/* port/net/net.c: a lockstep session (net.h; docs/port/NETWORK.md). Written from docs/port/NETPLAY.md's plan
 * and public documentation (RFC 7693 for BLAKE2s, NTP's offset and delay arithmetic, the sockets APIs). */
#include "net.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_blake2s.h"
#include "net_os.h"
#include "net_proto.h"
#include "net_sim.h"

#define RING 1024u   /* inputs, hashes and the schedule kept per side; far more than the window can need */
#define RMASK (RING - 1)
#define MS 1000000ull
#define NTP_BUCKETS 32   /* the clock offset: the least-delayed sample of each of the last 32 seconds */
#define DRIFT_PPM 50     /* an older sample's offset may be off by this much a second (clock drift) */
#define MAXWIN 512
#define WHASH_RING 16
#define PENDING 4      /* host: HELLOs answered, waiting for their sender to use the session (it proves itself) */

#define RESEND_NS (100 * MS)        /* reliable messages: every 100 ms until acknowledged */
#define PING_NS (50 * MS)           /* the handshake's pings */
#define HELLO_NS (100 * MS)
#define TRANSIT_WINDOW_NS (3000 * MS)   /* the delay follows the slowest input of the last 3 s */
#define SHRINK_GOOD_NS (2000 * MS)  /* a shrink needs 2 s of a smaller target, and 2 s since the last change */
#define LATE_PUMP_NS (50 * MS)      /* a pump this late: what it reads arrived at unknown times */
#define STALL_NS (1 * MS)           /* waiting less than this for the peer's input isn't a stall */
#define SYNC_DEADBAND_NS (1 * MS)
#define SYNC_MAX_NS (1 * MS)        /* time sync stretches a retrace by at most this */

typedef struct LocalSlot {
    uint32_t n;
    uint64_t t_assign;
    NetPad pad[NET_MAX_PORTS];   /* the local ports, in port order */
} LocalSlot;

typedef struct RemoteSlot {
    uint32_t n;
    uint8_t have;
    uint64_t t_arr;
    NetPad pad[NET_MAX_PORTS];   /* the peer's ports, in port order */
} RemoteSlot;

typedef struct HashSlot {
    uint32_t n, h;
    uint8_t have;
} HashSlot;

typedef struct Sched {
    uint32_t n;
    uint64_t t_start, t_ready;   /* submitted; its pads given (0: not yet) */
} Sched;

typedef struct WHash {
    uint32_t n;
    uint8_t have;
    uint64_t h;
} WHash;

typedef struct Pending {
    int used;
    NetAddr addr;
    uint64_t nonce, session, at;
    NetHello hello;
} Pending;

typedef struct NtpSample {
    uint64_t t, sec;
    int64_t delay, theta;
    int used;
} NtpSample;

typedef struct MaxWin {   /* sliding-window maximum by time: a deque with decreasing values */
    uint64_t t[MAXWIN];
    int64_t v[MAXWIN];
    int head, n;
} MaxWin;

typedef struct Blob {   /* one of this side's blobs (to send), or one of the peer's (to receive) */
    uint8_t *data;
    uint32_t size;
    uint8_t hash[NET_HASH_LEN];
    int nchunks, ngot, complete, ack_pending;
    uint8_t *bits;        /* acknowledged (sending) or received (receiving), one bit a chunk */
    uint64_t *sent_at;    /* sending: when each chunk last went out */
} Blob;

struct NetSession {
    NetMutex lock;
    NetCond cond;
    NetThread thread;
    int thread_on, stop;
    NetConfig cfg;
    char *build, *peer_str, *bind_str;
    uint8_t key[32];
    int keyed;
    NetTransport tr;
    NetSock sock;
    int own_sock;
    NetAddr peer;
    int peer_set;
    NetRole role;
    uint8_t ports, peer_ports;
    int nlocal, npeer;
    NetState state;
    NetEnd end;
    uint32_t end_detail;
    char end_msg[200];
    uint64_t created, handshake_at;
    uint64_t session_id, nonce, peer_nonce;
    /* sending */
    uint32_t tx_seq;
    uint64_t last_input_tx, last_hello_tx, last_ping_tx, last_ready_tx, last_start_tx;
    /* receiving */
    uint64_t last_rx, echo_t, echo_rx_at;
    int echo_have;
    uint32_t rx_seq_max, rx_seq_first;
    uint64_t rx_bits, rx_unique;
    int rx_any;
    /* clock offset and round trip */
    NtpSample ntp[NTP_BUCKETS];
    int64_t theta, rtt_best, hs_rtt_max, rtt_min_all, rtt_max_all;
    int theta_valid, host_clock;   /* guest: theta is the host's (negated), the same on both sides */
    double rtt_avg, rtt_jit;
    /* the handshake */
    NetHello my_hello, peer_hello;
    Blob my_settings, my_files[NET_MAX_FILES];
    Blob peer_settings, peer_files[NET_MAX_FILES];
    int peer_nfiles;
    uint8_t agree[NET_HASH_LEN], peer_ready_hash[NET_HASH_LEN];
    int accepted, peer_ready, settings_differ, guest_input_seen, own_settings;
    uint64_t start_host, start_local;
    int start_known;
    /* lockstep */
    LocalSlot *lslot;
    RemoteSlot *rslot;
    HashSlot *lhash, *rhash;
    Sched *sched;
    uint32_t local_next, peer_ack, remote_next, submit_next, consume_next;
    NetPad last_assigned[NET_MAX_PORTS];
    int delay;
    uint64_t good_since, last_change;
    MaxWin transit;
    uint32_t need_n;
    uint64_t need_t;
    int need_active, wait_logged;
    uint32_t lhash_next, rhash_next, peer_hash_ack, hash_checked;
    WHash lwh[WHASH_RING], rwh[WHASH_RING];
    uint32_t peer_whash_ack, my_whash_ack;
    /* the peer's latest packet (by sequence number) */
    int peer_latest_have;
    uint32_t peer_latest_seq, peer_cur;
    uint64_t peer_cur_start;
    int peer_delay;
    int64_t peer_feedback, adv;   /* adv: the smoothed advantage (time sync) */
    int adv_have;
    int peer_feedback_have;
    /* pumping */
    uint64_t last_pump;
    int frozen_was, misuse_logged;
    double hello_tokens, bye_tokens;
    Pending pending[PENDING];
    uint64_t tokens_at;
    NetSim *sim;
    NetStats st;
    uint8_t rxbuf[2048];
};

/* ---------------------------------------------------------------- small things */

static uint64_t now_ns(NetSession *s) { return s->cfg.now_ns ? s->cfg.now_ns(s->cfg.user) : net_os_now_ns(); }

static void say(NetSession *s, int important, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

static void say(NetSession *s, int important, const char *fmt, ...)
{
    char line[512];
    int at;
    va_list ap;
    if (s->cfg.quiet && !important)
        return;
    at = snprintf(line, sizeof line, "%s: ", s->role == NET_HOST ? "host" : "guest");
    va_start(ap, fmt);
    vsnprintf(line + at, sizeof line - (size_t)at, fmt, ap);
    va_end(ap);
    if (s->cfg.log)
        s->cfg.log(s->cfg.user, line);
    else
        fprintf(stderr, "net: %s\n", line);
}

static int64_t sdiff(uint64_t a, uint64_t b) { return (int64_t)(a - b); }

const char *net_state_name(NetState st)
{
    static const char *n[] = {"connecting", "handshake", "accept", "ready", "starting", "running", "ended"};
    return (unsigned)st <= NET_ENDED ? n[st] : "?";
}

const char *net_end_name(NetEnd e)
{
    static const char *n[NET_END_COUNT] = {"none",     "local",   "peer-left", "timeout",  "no-answer", "protocol",
                                           "desync",   "version", "mismatch",  "rejected", "busy",      "socket"};
    return (unsigned)e < NET_END_COUNT ? n[e] : "?";
}

void net_config_defaults(NetConfig *c)
{
    memset(c, 0, sizeof *c);
    c->role = NET_HOST;
    c->port = NET_DEFAULT_PORT;
    c->ports = 0x1;
    c->build = "unknown";
    c->delay_min = 1;
    c->delay_cap = 30;
    c->delay_margin_ms = 3;
    c->retrace_ns = 1000000000 / 60;
    c->timeout_ms = 15000;
    c->wait_report_ms = 250;
}

/* ---------------------------------------------------------------- sending */

static void begin(NetSession *s, NetWriter *w, uint8_t *buf, int type, uint64_t now, int with_session)
{
    NetHeader h;
    memset(&h, 0, sizeof h);
    h.version = NET_PROTO_VERSION;
    h.type = (uint8_t)type;
    h.flags = s->keyed ? NET_FLAG_TAG : 0;
    h.sender = s->role == NET_HOST ? 0 : 1;
    h.session = with_session ? s->session_id : 0;
    h.seq = ++s->tx_seq;
    h.t_send = now;
    if (s->echo_have) {
        uint64_t hold = (now - s->echo_rx_at) / 1000;
        h.echo = s->echo_t;
        h.hold_us = hold > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)hold;
    }
    net_w_init(w, buf, NET_MAX_PACKET - (s->keyed ? NET_TAG_SIZE : 0));
    net_w_header(w, &h);
}

static void finish_to(NetSession *s, NetWriter *w, const NetAddr *to, uint64_t now)
{
    if (w->over) {   /* a bug: every writer sizes itself */
        say(s, 1, "a %s packet didn't fit", net_msg_name(w->p[5]));
        return;
    }
    int len = w->len;
    if (s->keyed) {
        net_proto_tag(s->key, w->p, len, w->p + len);
        len += NET_TAG_SIZE;
    }
    if (s->sim && net_sim_frozen(s->sim, now))
        return;   /* a stopped process sends nothing */
    s->tr.send(s->tr.ctx, to, w->p, len);
    s->st.sent++;
    s->st.bytes_sent += (uint64_t)len;
}

static void finish(NetSession *s, NetWriter *w, uint64_t now) { finish_to(s, w, &s->peer, now); }

/* HELLO (guest), or HELLO_ACK (host) with the session it offers that guest and the guest's nonce. */
static void send_hello(NetSession *s, int type, const NetAddr *to, uint64_t session, uint64_t nonce_echo,
                       uint64_t now)
{
    uint8_t buf[NET_MAX_PACKET];
    NetWriter w;
    begin(s, &w, buf, type, now, 0);
    if (type == NET_MSG_HELLO_ACK)   /* the session field: the one offered */
        for (int i = 0; i < 8; i++)
            buf[8 + i] = (uint8_t)(session >> (8 * i));
    NetHello h = s->my_hello;
    h.nonce_echo = nonce_echo;
    net_w_hello(&w, &h);
    finish_to(s, &w, to, now);
}

static void send_bye_to(NetSession *s, const NetAddr *to, NetEnd why, uint32_t detail, const char *text, int session,
                        uint64_t now)
{
    uint8_t buf[NET_MAX_PACKET];
    NetWriter w;
    NetBye b;
    memset(&b, 0, sizeof b);
    b.reason = (uint8_t)why;
    b.detail = detail;
    snprintf(b.text, sizeof b.text, "%s", text ? text : "");
    begin(s, &w, buf, NET_MSG_BYE, now, session);
    net_w_bye(&w, &b);
    finish_to(s, &w, to, now);
}

static void send_simple(NetSession *s, int type, const void *payload, int n, uint64_t now)
{
    uint8_t buf[NET_MAX_PACKET];
    NetWriter w;
    begin(s, &w, buf, type, now, 1);
    net_w_bytes(&w, payload, n);
    finish(s, &w, now);
}

static void send_ping(NetSession *s, int type, uint64_t now)
{
    uint8_t id[4] = {0};
    send_simple(s, type, id, 4, now);
    if (type == NET_MSG_PING)
        s->last_ping_tx = now;
}

static int64_t window_max(MaxWin *m, uint64_t now, int64_t window)
{
    while (m->n > 0 && sdiff(now, m->t[m->head]) > window) {
        m->head = (m->head + 1) % MAXWIN;
        m->n--;
    }
    return m->n ? m->v[m->head] : -1;
}

static void window_push(MaxWin *m, uint64_t t, int64_t v)
{
    while (m->n > 0 && m->v[(m->head + m->n - 1) % MAXWIN] <= v)
        m->n--;
    if (m->n == MAXWIN) {
        m->head = (m->head + 1) % MAXWIN;
        m->n--;
    }
    int i = (m->head + m->n) % MAXWIN;
    m->t[i] = t;
    m->v[i] = v;
    m->n++;
}

/* INPUT: every input the peer hasn't acknowledged, the acks, the schedule, the hashes the peer hasn't acknowledged,
 * the feedback on its inputs' transit, and a window hash when one waits. */
static void send_input(NetSession *s, uint64_t now)
{
    uint8_t buf[NET_MAX_PACKET];
    NetWriter w;
    begin(s, &w, buf, NET_MSG_INPUT, now, 1);
    WHash *wh = NULL;
    for (int i = 0; i < WHASH_RING; i++) {   /* the oldest window hash the peer hasn't had */
        WHash *x = &s->lwh[i];
        if (x->have && x->n + 1 > s->peer_whash_ack && (!wh || x->n < wh->n))
            wh = x;
    }
    int room = w.cap - w.len - 52 - (wh ? 12 : 0);
    int per = s->nlocal * 4 + 2;
    uint32_t unacked = s->local_next - s->peer_ack;
    int count = (int)(unacked < NET_WINDOW ? unacked : NET_WINDOW);
    if (count * per > room)
        count = room / per;
    room -= count * per;
    uint32_t hfirst = s->peer_hash_ack;   /* every hash the peer hasn't acknowledged */
    if (hfirst > s->lhash_next)
        hfirst = s->lhash_next;
    if (s->lhash_next - hfirst > RING - 8)   /* only what the ring still has */
        hfirst = s->lhash_next - (RING - 8);
    uint32_t hpend = s->lhash_next - hfirst;
    int hcount = (int)(hpend < NET_MAX_HASHES ? hpend : NET_MAX_HASHES);
    if (hcount * 4 > room)
        hcount = room / 4;
    uint32_t cur = s->submit_next ? s->submit_next - 1 : NET_RETRACE_NONE;
    int64_t fb = window_max(&s->transit, now, TRANSIT_WINDOW_NS);
    net_w_u32(&w, cur);
    net_w_u64(&w, cur == NET_RETRACE_NONE ? 0 : s->sched[cur & RMASK].t_start);
    net_w_u32(&w, s->remote_next);
    net_w_u32(&w, s->rhash_next);
    net_w_u32(&w, s->my_whash_ack);
    net_w_u32(&w, s->peer_ack);
    net_w_u8(&w, (uint8_t)count);
    net_w_u8(&w, s->ports);
    net_w_u8(&w, (uint8_t)(s->delay > 255 ? 255 : s->delay));
    net_w_u8(&w, (uint8_t)((wh ? NET_INPUT_WHASH : 0) | (fb >= 0 ? NET_INPUT_FEEDBACK : 0) |
                           (s->theta_valid ? NET_INPUT_CLOCK : 0)));
    net_w_u32(&w, fb >= 0 ? (uint32_t)(fb / 1000 > 0xFFFFFFF0 ? 0xFFFFFFF0 : fb / 1000) : 0);
    net_w_u64(&w, s->theta_valid ? (uint64_t)s->theta : 0);
    net_w_u32(&w, hfirst);
    net_w_u16(&w, (uint16_t)hcount);
    net_w_u16(&w, 0);
    for (int i = 0; i < count; i++) {
        const LocalSlot *ls = &s->lslot[(s->peer_ack + (uint32_t)i) & RMASK];
        for (int k = 0; k < s->nlocal; k++) {
            net_w_u16(&w, ls->pad[k].buttons);
            net_w_u8(&w, (uint8_t)ls->pad[k].stick_x);
            net_w_u8(&w, (uint8_t)ls->pad[k].stick_y);
        }
    }
    for (int i = 0; i < count; i++) {
        uint64_t age = (now - s->lslot[(s->peer_ack + (uint32_t)i) & RMASK].t_assign) / 100000;
        net_w_u16(&w, (uint16_t)(age > 0xFFFF ? 0xFFFF : age));
    }
    for (int i = 0; i < hcount; i++)
        net_w_u32(&w, s->lhash[(hfirst + (uint32_t)i) & RMASK].h);
    if (wh) {
        net_w_u32(&w, wh->n);
        net_w_u64(&w, wh->h);
    }
    finish(s, &w, now);
    s->last_input_tx = now;
}

/* ---------------------------------------------------------------- ending */

static void end_session(NetSession *s, NetEnd why, uint32_t detail, int bye, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;

static void end_session(NetSession *s, NetEnd why, uint32_t detail, int bye, const char *fmt, ...)
{
    va_list ap;
    if (s->state == NET_ENDED)
        return;
    uint64_t now = now_ns(s);
    s->state = NET_ENDED;
    s->end = why;
    s->end_detail = detail;
    va_start(ap, fmt);
    vsnprintf(s->end_msg, sizeof s->end_msg, fmt, ap);
    va_end(ap);
    say(s, 1, "session ended (%s): %s", net_end_name(why), s->end_msg);
    if (bye && s->peer_set && s->session_id)
        for (int i = 0; i < 3; i++)   /* three copies: a lost BYE costs the peer its timeout */
            send_bye_to(s, &s->peer, why, detail, why == NET_END_VERSION ? s->build : s->end_msg, 1, now);
    s->need_active = 0;
    net_cond_broadcast(&s->cond);
}

/* ---------------------------------------------------------------- the clock offset (NTP's arithmetic) */

static void ntp_sample(NetSession *s, const NetHeader *h, uint64_t now)
{
    if (!h->echo)
        return;
    uint64_t t1 = h->echo, t3 = h->t_send, t4 = now, hold = (uint64_t)h->hold_us * 1000;
    uint64_t t2 = t3 - hold;
    int64_t delay = sdiff(t4, t1) - (int64_t)hold;
    if (sdiff(t4, t1) < 0 || sdiff(t1, s->created) < 0 || delay < 0 || delay > 10000 * (int64_t)MS)
        return;   /* not a time this session sent, or nonsense */
    int64_t theta = (sdiff(t2, t1) + sdiff(t3, t4)) / 2;   /* the peer's clock minus this side's */
    uint64_t sec = now / 1000000000u;
    NtpSample *x = &s->ntp[sec % NTP_BUCKETS];
    if (!x->used || x->sec != sec || delay < x->delay) {
        x->used = 1;
        x->sec = sec;
        x->t = now;
        x->delay = delay;
        x->theta = theta;
    }
    /* The least-delayed sample has the truest offset (its two directions were both quick, so nearly equal). An
     * older one counts as DRIFT_PPM worse per second of age, as the clocks drift apart. Only whole seconds count
     * (the current one only before there is any): a lone lopsided sample, as when the latency drops and one
     * direction is still slow, is beaten within its second by the samples after it. */
    const NtpSample *best = NULL;
    int64_t best_score = 0;
    int whole = 0;
    for (int i = 0; i < NTP_BUCKETS; i++)
        whole |= s->ntp[i].used && s->ntp[i].sec != sec;
    for (int i = 0; i < NTP_BUCKETS; i++) {
        const NtpSample *c = &s->ntp[i];
        int64_t age = sdiff(now, c->t);
        if (!c->used || age > (int64_t)NTP_BUCKETS * 1000 * (int64_t)MS || (whole && c->sec == sec))
            continue;
        int64_t score = c->delay + age / 1000000 * DRIFT_PPM;
        if (!best || score < best_score) {
            best = c;
            best_score = score;
        }
    }
    if (best) {
        s->rtt_best = best->delay;
        if (!s->host_clock) {
            s->theta = best->theta;
            s->theta_valid = 1;
        }
    }
    double d = (double)delay / 1e6;
    if (!s->st.rtt_samples) {
        s->rtt_avg = d;
        s->rtt_min_all = s->rtt_max_all = delay;
    }
    s->rtt_jit += ((d > s->rtt_avg ? d - s->rtt_avg : s->rtt_avg - d) - s->rtt_jit) / 16;
    s->rtt_avg += (d - s->rtt_avg) / 16;
    if (delay < s->rtt_min_all)
        s->rtt_min_all = delay;
    if (delay > s->rtt_max_all)
        s->rtt_max_all = delay;
    if (s->state < NET_STARTING && delay > s->hs_rtt_max)
        s->hs_rtt_max = delay;
    s->st.rtt_samples++;
}

/* ---------------------------------------------------------------- the handshake */

static void hash_blob_info(NetBlake2s *c, const NetBlobInfo *b)
{
    uint8_t sz[4] = {(uint8_t)b->size, (uint8_t)(b->size >> 8), (uint8_t)(b->size >> 16), (uint8_t)(b->size >> 24)};
    net_blake2s_update(c, sz, 4);
    net_blake2s_update(c, b->hash, NET_HASH_LEN);
}

/* What both sides agree on, the same bytes on both: the build, the ports, the host's settings, every file. */
static void compute_agreement(NetSession *s)
{
    const NetHello *host = s->role == NET_HOST ? &s->my_hello : &s->peer_hello;
    const NetHello *guest = s->role == NET_HOST ? &s->peer_hello : &s->my_hello;
    static const char label[] = "rush2049pc agreement v1";
    char build[NET_BUILD_LEN] = {0};
    uint8_t v = NET_PROTO_VERSION;
    NetBlake2s c;
    net_blake2s_init(&c, NET_HASH_LEN, NULL, 0);
    net_blake2s_update(&c, label, sizeof label);
    net_blake2s_update(&c, &v, 1);
    snprintf(build, sizeof build, "%s", host->build);
    net_blake2s_update(&c, build, sizeof build);
    net_blake2s_update(&c, &host->ports, 1);
    net_blake2s_update(&c, &guest->ports, 1);
    hash_blob_info(&c, &host->settings);
    net_blake2s_update(&c, &host->nfiles, 1);
    for (int i = 0; i < host->nfiles; i++)
        hash_blob_info(&c, &host->files[i]);
    net_blake2s_update(&c, &guest->nfiles, 1);
    for (int i = 0; i < guest->nfiles; i++)
        hash_blob_info(&c, &guest->files[i]);
    net_blake2s_final(&c, s->agree);
}

static int blob_alloc(Blob *b, uint32_t size, const uint8_t *hash, int sending)
{
    memset(b, 0, sizeof *b);
    b->size = size;
    if (hash)
        memcpy(b->hash, hash, NET_HASH_LEN);
    b->nchunks = (int)((size + NET_CHUNK - 1) / NET_CHUNK);
    if (!size) {
        b->complete = 1;
        return 0;
    }
    b->data = malloc(size);
    b->bits = calloc((size_t)(b->nchunks + 7) / 8, 1);
    b->sent_at = sending ? calloc((size_t)b->nchunks, sizeof *b->sent_at) : NULL;
    return b->data && b->bits && (!sending || b->sent_at) ? 0 : -1;
}

static void blob_free(Blob *b)
{
    free(b->data);
    free(b->bits);
    free(b->sent_at);
    memset(b, 0, sizeof *b);
}

static int bit(const uint8_t *bits, int i) { return bits[i >> 3] >> (i & 7) & 1; }
static void set_bit(uint8_t *bits, int i) { bits[i >> 3] |= (uint8_t)(1u << (i & 7)); }

/* Sizes the peer announced: within the limits, or the session can't go on. */
static int hello_sizes_ok(const NetHello *h, int from_host)
{
    if (from_host && h->settings.size > NET_SETTINGS_MAX_SIZE)
        return 0;
    for (int i = 0; i < h->nfiles; i++)
        if (h->files[i].size > NET_FILE_MAX_SIZE)
            return 0;
    return 1;
}

/* The peer's blobs, from its HELLO or HELLO_ACK. */
static int setup_peer_blobs(NetSession *s)
{
    const NetHello *h = &s->peer_hello;
    if (s->role == NET_GUEST && blob_alloc(&s->peer_settings, h->settings.size, h->settings.hash, 0) != 0)
        return -1;
    if (s->role == NET_HOST)
        s->peer_settings.complete = 1;
    s->peer_nfiles = h->nfiles;
    for (int i = 0; i < h->nfiles; i++)
        if (blob_alloc(&s->peer_files[i], h->files[i].size, h->files[i].hash, 0) != 0)
            return -1;
    return 0;
}

static void accept_locked(NetSession *s, uint64_t now)
{
    if (s->state != NET_ACCEPT)
        return;
    s->state = NET_READY;
    s->accepted = 1;
    send_simple(s, NET_MSG_READY, s->agree, NET_HASH_LEN, now);
    s->last_ready_tx = now;
}

static void check_received(NetSession *s, uint64_t now)
{
    if (s->state != NET_HANDSHAKE)
        return;
    if (!s->peer_settings.complete)
        return;
    for (int i = 0; i < s->peer_nfiles; i++)
        if (!s->peer_files[i].complete)
            return;
    s->state = NET_ACCEPT;
    say(s, 0, "settings and %d file(s) received%s", s->peer_nfiles,
        s->settings_differ ? " (the host's settings differ from this side's: the host's are used)" : "");
    net_cond_broadcast(&s->cond);
    if (s->cfg.auto_accept)
        accept_locked(s, now);
}

static void enter_handshake(NetSession *s, uint64_t now)
{
    s->state = NET_HANDSHAKE;
    s->handshake_at = now;
    compute_agreement(s);
    net_cond_broadcast(&s->cond);
    check_received(s, now);
}

static void on_hello(NetSession *s, const NetMsg *m, const NetAddr *from, uint64_t now)
{
    const NetHello *h = &m->u.hello;
    char a[80];
    net_addr_str(from, a, sizeof a);
    if (s->role != NET_HOST) {
        s->st.rejected++;
        return;
    }
    if (s->hello_tokens < 1) {
        s->st.hellos_limited++;
        return;
    }
    s->hello_tokens -= 1;
    if (s->peer_set) {   /* the peer again: its HELLO_ACK was lost */
        if (h->nonce == s->peer_nonce && s->state != NET_ENDED)
            send_hello(s, NET_MSG_HELLO_ACK, &s->peer, s->session_id, s->peer_nonce, now);
        return;
    }
    if (strcmp(h->build, s->build) != 0) {
        say(s, 1, "a guest at %s runs build %s; this is %s: it can't join", a, h->build, s->build);
        send_bye_to(s, from, NET_END_VERSION, 0, s->build, 0, now);
        return;
    }
    if (h->ports & s->ports) {
        say(s, 1, "a guest at %s wants ports 0x%x, which overlap this side's 0x%x", a, h->ports, s->ports);
        send_bye_to(s, from, NET_END_PROTOCOL, 0, "the ports overlap", 0, now);
        return;
    }
    if (!hello_sizes_ok(h, 0)) {
        send_bye_to(s, from, NET_END_PROTOCOL, 0, "a file is too large", 0, now);
        return;
    }
    /* Not the peer yet: a HELLO can be replayed from anywhere. Each sender gets its own session in a HELLO_ACK,
     * and the first to send a packet in its session (tagged, with a join code) becomes the peer (confirm_peer). */
    Pending *p = NULL;
    for (int i = 0; i < PENDING; i++)
        if (s->pending[i].used && net_addr_equal(&s->pending[i].addr, from))
            p = &s->pending[i];
    if (p && p->nonce == h->nonce) {   /* its HELLO_ACK was lost */
        send_hello(s, NET_MSG_HELLO_ACK, from, p->session, p->nonce, now);
        return;
    }
    if (!p) {
        p = &s->pending[0];
        for (int i = 0; i < PENDING; i++) {   /* a free entry, else the oldest */
            if (!s->pending[i].used) {
                p = &s->pending[i];
                break;
            }
            if (s->pending[i].at < p->at)
                p = &s->pending[i];
        }
    }
    p->used = 1;
    p->addr = *from;
    p->nonce = h->nonce;
    p->hello = *h;
    p->at = now;
    do
        net_os_random(&p->session, sizeof p->session);
    while (!p->session);
    send_hello(s, NET_MSG_HELLO_ACK, from, p->session, p->nonce, now);
}

/* host: a packet in a pending session from its address: that guest is the peer. */
static int confirm_peer(NetSession *s, const NetMsg *m, const NetAddr *from, uint64_t now)
{
    Pending *p = NULL;
    for (int i = 0; i < PENDING; i++)
        if (s->pending[i].used && net_addr_equal(&s->pending[i].addr, from) && s->pending[i].session == m->h.session)
            p = &s->pending[i];
    if (!p || m->h.type == NET_MSG_HELLO)
        return 0;
    const NetHello *h = &p->hello;
    char a[80];
    net_addr_str(from, a, sizeof a);
    s->peer = *from;
    s->peer_set = 1;
    s->session_id = p->session;
    s->peer_nonce = p->nonce;
    s->peer_hello = *h;
    s->peer_ports = h->ports;
    s->npeer = net_popcount4(h->ports);
    memset(s->pending, 0, sizeof s->pending);
    h = &s->peer_hello;
    if (setup_peer_blobs(s) != 0) {
        end_session(s, NET_END_PROTOCOL, 0, 1, "out of memory for the guest's files");
        return 1;
    }
    say(s, 0, "a guest joined from %s (ports 0x%x, %d file(s))", a, h->ports, h->nfiles);
    enter_handshake(s, now);
    return 1;
}

static void on_hello_ack(NetSession *s, const NetMsg *m, uint64_t now)
{
    const NetHello *h = &m->u.hello;
    if (s->role != NET_GUEST || s->state != NET_CONNECTING)
        return;   /* a repeat */
    if (h->nonce_echo != s->nonce || !m->h.session) {
        s->st.rejected++;
        return;
    }
    s->session_id = m->h.session;
    if (strcmp(h->build, s->build) != 0) {
        end_session(s, NET_END_VERSION, 0, 1, "the host runs build %s; this is %s: both players need the same build",
                    h->build, s->build);
        return;
    }
    if (h->ports & s->ports) {
        end_session(s, NET_END_PROTOCOL, 0, 1, "the host's ports 0x%x overlap this side's 0x%x", h->ports, s->ports);
        return;
    }
    if (!hello_sizes_ok(h, 1)) {
        end_session(s, NET_END_PROTOCOL, 0, 1, "the host's settings or files are too large");
        return;
    }
    s->peer_nonce = h->nonce;
    s->peer_hello = *h;
    s->peer_ports = h->ports;
    s->npeer = net_popcount4(h->ports);
    if (s->own_settings && (s->my_hello.settings.size != h->settings.size ||
                            memcmp(s->my_hello.settings.hash, h->settings.hash, NET_HASH_LEN) != 0))
        s->settings_differ = 1;
    if (setup_peer_blobs(s) != 0) {
        end_session(s, NET_END_PROTOCOL, 0, 1, "out of memory for the host's files");
        return;
    }
    say(s, 0, "connected (ports 0x%x, the host's settings %u bytes, %d file(s))", h->ports, h->settings.size,
        h->nfiles);
    enter_handshake(s, now);
}

static void on_chunk(NetSession *s, const NetMsg *m, uint64_t now)
{
    const NetChunk *c = &m->u.chunk;
    Blob *b = NULL;
    if (m->h.type == NET_MSG_SETTINGS) {
        if (s->role == NET_GUEST && c->index == 0)
            b = &s->peer_settings;
    } else if (c->index < s->peer_nfiles) {
        b = &s->peer_files[c->index];
    }
    if (!b || !b->size || c->total != b->size || c->chunk >= b->nchunks ||
        c->len != (c->chunk == b->nchunks - 1 ? b->size - (uint32_t)c->chunk * NET_CHUNK : NET_CHUNK)) {
        s->st.rejected++;
        return;
    }
    b->ack_pending = 1;
    if (bit(b->bits, c->chunk))
        return;
    memcpy(b->data + (size_t)c->chunk * NET_CHUNK, c->data, c->len);
    set_bit(b->bits, c->chunk);
    if (++b->ngot < b->nchunks)
        return;
    uint8_t h[NET_HASH_LEN];
    net_blake2s(h, NET_HASH_LEN, NULL, 0, b->data, b->size);
    if (memcmp(h, b->hash, NET_HASH_LEN) != 0) {
        end_session(s, NET_END_PROTOCOL, 0, 1, "the peer's %s %d doesn't match its hash",
                    m->h.type == NET_MSG_SETTINGS ? "settings" : "file", c->index);
        return;
    }
    b->complete = 1;
    check_received(s, now);
}

static void on_chunk_ack(NetSession *s, const NetMsg *m)
{
    const NetChunkAck *a = &m->u.chunk_ack;
    Blob *b = NULL;
    if (a->kind == NET_BLOB_SETTINGS) {
        if (s->role == NET_HOST && a->index == 0)
            b = &s->my_settings;
    } else if (a->index < s->my_hello.nfiles) {
        b = &s->my_files[a->index];
    }
    if (!b || !b->size || a->nchunks != b->nchunks) {
        s->st.rejected++;
        return;
    }
    for (int i = 0; i < (b->nchunks + 7) / 8; i++)
        b->bits[i] |= a->bitmap[i];
}

static void send_blob_chunks(NetSession *s, Blob *b, int type, int index, uint64_t now, int *budget)
{
    for (int c = 0; c < b->nchunks && *budget > 0; c++) {
        if (bit(b->bits, c) || (b->sent_at[c] && now - b->sent_at[c] < RESEND_NS))
            continue;
        if (b->sent_at[c])
            s->st.resends++;
        b->sent_at[c] = now;
        (*budget)--;
        NetChunk ch = {(uint8_t)index, (uint16_t)c, b->size,
                       (uint16_t)(c == b->nchunks - 1 ? b->size - (uint32_t)c * NET_CHUNK : NET_CHUNK),
                       b->data + (size_t)c * NET_CHUNK};
        uint8_t buf[NET_MAX_PACKET];
        NetWriter w;
        begin(s, &w, buf, type, now, 1);
        net_w_chunk(&w, &ch);
        finish(s, &w, now);
    }
}

static void send_chunk_ack(NetSession *s, Blob *b, int kind, int index, uint64_t now)
{
    uint8_t buf[NET_MAX_PACKET];
    NetWriter w;
    b->ack_pending = 0;
    begin(s, &w, buf, NET_MSG_FILE_ACK, now, 1);
    net_w_u8(&w, (uint8_t)kind);
    net_w_u8(&w, (uint8_t)index);
    net_w_u16(&w, (uint16_t)b->nchunks);
    net_w_bytes(&w, b->bits, (b->nchunks + 7) / 8);
    finish(s, &w, now);
}

static void transfer(NetSession *s, uint64_t now)
{
    int budget = 32;   /* chunks a pump, so a slow link isn't flooded */
    if (s->role == NET_HOST && s->my_settings.size)
        send_blob_chunks(s, &s->my_settings, NET_MSG_SETTINGS, 0, now, &budget);
    for (int i = 0; i < s->my_hello.nfiles; i++)
        if (s->my_files[i].size)
            send_blob_chunks(s, &s->my_files[i], NET_MSG_FILE, i, now, &budget);
    if (s->peer_settings.ack_pending)
        send_chunk_ack(s, &s->peer_settings, NET_BLOB_SETTINGS, 0, now);
    for (int i = 0; i < s->peer_nfiles; i++)
        if (s->peer_files[i].ack_pending)
            send_chunk_ack(s, &s->peer_files[i], NET_BLOB_FILE, i, now);
}

/* ---------------------------------------------------------------- the start */

static int initial_delay(NetSession *s)
{
    if (s->cfg.delay_fixed > 0)
        return s->cfg.delay_fixed;
    int64_t need = s->hs_rtt_max / 2 + (int64_t)(s->cfg.delay_margin_ms * 1e6);
    int d = (int)((need + s->cfg.retrace_ns - 1) / s->cfg.retrace_ns);
    if (d < s->cfg.delay_min)
        d = s->cfg.delay_min;
    if (d > s->cfg.delay_cap)
        d = s->cfg.delay_cap;
    return d;
}

/* Retraces 0 .. D-1 have idle input on this side: assigned now and sent before retrace 0. */
static void prefill(NetSession *s, uint64_t now)
{
    s->delay = initial_delay(s);
    s->st.delay_min_seen = s->st.delay_max_seen = s->delay;
    s->last_change = now;
    memset(s->last_assigned, 0, sizeof s->last_assigned);
    for (uint32_t n = 0; n < (uint32_t)s->delay; n++) {
        LocalSlot *ls = &s->lslot[n & RMASK];
        ls->n = n;
        ls->t_assign = now;
        memset(ls->pad, 0, sizeof ls->pad);
    }
    s->local_next = (uint32_t)s->delay;
    say(s, 0, "input delay %d to start (round trip up to %.1f ms in the handshake)", s->delay, s->hs_rtt_max / 1e6);
}

/* START: the agreement, retrace 0's time on the host's clock, and the host's clock offset for the guest to use. */
static void send_start(NetSession *s, uint64_t now)
{
    uint8_t buf[NET_MAX_PACKET];
    NetWriter w;
    begin(s, &w, buf, NET_MSG_START, now, 1);
    net_w_bytes(&w, s->agree, NET_HASH_LEN);
    net_w_u64(&w, s->start_host);
    net_w_u64(&w, s->theta_valid ? (uint64_t)s->theta : 0);
    net_w_u8(&w, (uint8_t)(s->theta_valid ? 1 : 0));
    net_w_bytes(&w, NULL, 3);
    finish(s, &w, now);
    s->last_start_tx = now;
}

static void try_start(NetSession *s, uint64_t now)
{
    if (s->role != NET_HOST || s->state != NET_READY || !s->peer_ready)
        return;
    if (s->st.rtt_samples < 4 && now - s->handshake_at < 2000 * MS)
        return;   /* a few round trips first, for the offset and the first delay */
    if (memcmp(s->peer_ready_hash, s->agree, NET_HASH_LEN) != 0) {
        end_session(s, NET_END_MISMATCH, 0, 1, "the guest's agreement differs from this side's");
        return;
    }
    int64_t lead = 2 * s->hs_rtt_max + 100 * (int64_t)MS;
    if (lead < 250 * (int64_t)MS)
        lead = 250 * (int64_t)MS;
    if (lead > 3000 * (int64_t)MS)
        lead = 3000 * (int64_t)MS;
    s->start_host = s->start_local = now + (uint64_t)lead;
    s->start_known = 1;
    s->state = NET_STARTING;
    prefill(s, now);
    send_start(s, now);
    send_input(s, now);
    say(s, 0, "starting in %.0f ms", lead / 1e6);
    net_cond_broadcast(&s->cond);
}

static void on_start(NetSession *s, const NetMsg *m, uint64_t now)
{
    if (s->role != NET_GUEST || s->state >= NET_STARTING)
        return;   /* a repeat */
    if (s->state != NET_READY) {
        s->st.rejected++;
        return;
    }
    if (memcmp(m->u.start.hash, s->agree, NET_HASH_LEN) != 0) {
        end_session(s, NET_END_MISMATCH, 0, 1, "the host's agreement differs from this side's");
        return;
    }
    s->start_host = m->u.start.start_host_ns;
    if (m->u.start.clock_valid) {   /* from here on the host's offset, so both sides use the same */
        s->theta = -m->u.start.clock;
        s->theta_valid = 1;
        s->host_clock = 1;
    }
    if (s->theta_valid) {
        s->start_local = s->start_host - (uint64_t)s->theta;
        if (sdiff(s->start_local, now) < 0)
            s->start_local = now;
        if (sdiff(s->start_local, now) > 5000 * (int64_t)MS)
            s->start_local = now + 5000 * MS;
    } else {
        s->start_local = now + 100 * MS;
    }
    s->start_known = 1;
    s->state = NET_STARTING;
    prefill(s, now);
    send_input(s, now);   /* also START's acknowledgement */
    say(s, 0, "starting in %.0f ms (clock offset %.3f ms)", sdiff(s->start_local, now) / 1e6, s->theta / 1e6);
    net_cond_broadcast(&s->cond);
}

/* ---------------------------------------------------------------- the lockstep */

static void check_hashes(NetSession *s)
{
    while (s->state != NET_ENDED && s->hash_checked < s->lhash_next) {
        uint32_t r = s->hash_checked;
        const HashSlot *l = &s->lhash[r & RMASK], *p = &s->rhash[r & RMASK];
        if (!p->have || p->n != r)
            break;
        if (l->h != p->h) {
            end_session(s, NET_END_DESYNC, r, 1,
                        "the two games went out of sync at retrace %u (this side's hash %08x, the peer's %08x)", r,
                        l->h, p->h);
            return;
        }
        s->st.hash_checks++;
        s->hash_checked++;
    }
}

static void check_whash(NetSession *s, uint32_t n)
{
    const WHash *l = NULL, *p = NULL;
    for (int i = 0; i < WHASH_RING; i++) {
        if (s->lwh[i].have && s->lwh[i].n == n)
            l = &s->lwh[i];
        if (s->rwh[i].have && s->rwh[i].n == n)
            p = &s->rwh[i];
    }
    if (!l || !p)
        return;
    if (l->h != p->h) {
        end_session(s, NET_END_DESYNC, n, 1, "the two games' windows differ at retrace %u", n);
        return;
    }
    s->st.window_hash_checks++;
}

static void on_input(NetSession *s, const NetMsg *m, uint64_t now, int late)
{
    const NetInput *in = &m->u.input;
    if (s->state < NET_STARTING)
        return;   /* sent after START, which hasn't arrived here: the host repeats START */
    if (s->role == NET_HOST)
        s->guest_input_seen = 1;
    uint32_t newest = in->first + in->count - 1;
    if (in->ports != s->peer_ports || in->ack > s->local_next ||
        (in->count && (newest >= s->consume_next + RING - 8 || (in->cur != NET_RETRACE_NONE && newest < in->cur))) ||
        (in->cur != NET_RETRACE_NONE && in->cur >= s->consume_next + RING - 8) ||
        (in->hash_count && (in->cur == NET_RETRACE_NONE || in->hash_first + in->hash_count > in->cur + 1 ||
                            in->hash_first + in->hash_count > s->hash_checked + RING - 8)) ||
        in->hash_ack > s->lhash_next) {
        s->st.rejected++;   /* out of any window this session can be in */
        return;
    }
    const uint8_t *p = in->pads;
    int got = 0;
    for (int i = 0; i < in->count; i++, p += 4 * s->npeer) {
        uint32_t n = in->first + (uint32_t)i;
        RemoteSlot *r = &s->rslot[n & RMASK];
        NetPad pads[NET_MAX_PORTS];
        memset(pads, 0, sizeof pads);
        for (int k = 0; k < s->npeer; k++) {
            pads[k].buttons = net_get16(p + 4 * k);
            pads[k].stick_x = (int8_t)p[4 * k + 2];
            pads[k].stick_y = (int8_t)p[4 * k + 3];
        }
        if (r->have && r->n == n) {   /* the first copy wins; another value is the peer's error */
            if (memcmp(r->pad, pads, sizeof pads) != 0) {
                end_session(s, NET_END_PROTOCOL, n, 1, "the peer sent two different inputs for retrace %u", n);
                return;
            }
            continue;
        }
        if (n < s->remote_next)
            continue;   /* consumed long ago; nothing to compare with */
        r->n = n;
        r->have = 1;
        r->t_arr = now;
        memcpy(r->pad, pads, sizeof pads);
        got = 1;
        if (!late && s->theta_valid) {   /* from its assignment there to its arrival here */
            uint64_t age = (uint64_t)net_get16(in->ages + 2 * i) * 100000;
            uint64_t assigned = m->h.t_send - age - (uint64_t)s->theta;   /* on this side's clock */
            int64_t transit = sdiff(now, assigned);
            /* the idle inputs assigned before the start waited for START here, not on the network */
            if (sdiff(assigned, s->start_local) >= 0 && transit > -1000 * (int64_t)MS &&
                transit < 10000 * (int64_t)MS)
                window_push(&s->transit, now, transit < 0 ? 0 : transit);
        }
    }
    while (s->rslot[s->remote_next & RMASK].have && s->rslot[s->remote_next & RMASK].n == s->remote_next)
        s->remote_next++;
    if (in->ack > s->peer_ack)
        s->peer_ack = in->ack;
    if (in->hash_ack > s->peer_hash_ack)
        s->peer_hash_ack = in->hash_ack;
    if (in->whash_ack > s->peer_whash_ack)
        s->peer_whash_ack = in->whash_ack;
    if (!s->peer_latest_have || (int32_t)(m->h.seq - s->peer_latest_seq) > 0) {
        s->peer_latest_have = 1;
        s->peer_latest_seq = m->h.seq;
        s->peer_cur = in->cur;
        s->peer_cur_start = in->cur_start;
        s->peer_delay = in->delay;
        if (in->flags & NET_INPUT_FEEDBACK) {
            s->peer_feedback = (int64_t)in->feedback_us * 1000;
            s->peer_feedback_have = 1;
        }
        if ((in->flags & NET_INPUT_CLOCK) && s->role == NET_GUEST) {
            s->theta = -in->clock;
            s->theta_valid = 1;
            s->host_clock = 1;
        }
    }
    for (int i = 0; i < in->hash_count; i++) {
        uint32_t r = in->hash_first + (uint32_t)i, v = net_get32(in->hashes + 4 * i);
        HashSlot *x = &s->rhash[r & RMASK];
        if (x->have && x->n == r) {
            if (x->h != v) {
                end_session(s, NET_END_PROTOCOL, r, 1, "the peer sent two different hashes for retrace %u", r);
                return;
            }
            continue;
        }
        if (r < s->hash_checked)
            continue;
        x->n = r;
        x->h = v;
        x->have = 1;
    }
    while (s->rhash[s->rhash_next & RMASK].have && s->rhash[s->rhash_next & RMASK].n == s->rhash_next)
        s->rhash_next++;
    check_hashes(s);
    if ((in->flags & NET_INPUT_WHASH) && s->state != NET_ENDED) {
        WHash *x = &s->rwh[in->whash_retrace % WHASH_RING];
        if (!(x->have && x->n == in->whash_retrace)) {
            x->n = in->whash_retrace;
            x->h = in->whash;
            x->have = 1;
            if (in->whash_retrace + 1 > s->my_whash_ack)
                s->my_whash_ack = in->whash_retrace + 1;
            check_whash(s, in->whash_retrace);
        } else if (x->h != in->whash) {
            end_session(s, NET_END_PROTOCOL, in->whash_retrace, 1, "the peer sent two window hashes for retrace %u",
                        in->whash_retrace);
        }
    }
    if (s->local_next - s->peer_ack > RING - 16 && s->state != NET_ENDED)
        end_session(s, NET_END_PROTOCOL, 0, 1, "the peer doesn't acknowledge this side's inputs");
    if (got)
        net_cond_broadcast(&s->cond);
}

/* The delay controller (docs/port/NETWORK.md 4): this side's delay covers the slowest transit of its inputs to the
 * peer in the last 3 s (the peer's feedback) or of the peer's inputs here (the same path backwards: the faster
 * signal), plus a margin. It grows at once; it shrinks by one only after the smaller target has held for 2 s,
 * and 2 s after the last change. */
static void update_delay(NetSession *s, uint64_t now)
{
    if (s->cfg.delay_fixed > 0) {
        s->delay = s->cfg.delay_fixed;
        return;
    }
    int64_t need = -1, local = window_max(&s->transit, now, TRANSIT_WINDOW_NS);
    if (s->peer_feedback_have)
        need = s->peer_feedback;
    if (local > need)
        need = local;
    if (need < 0)
        return;   /* nothing measured lately (a long stall): hold */
    need += (int64_t)(s->cfg.delay_margin_ms * 1e6);
    int target = (int)((need + s->cfg.retrace_ns - 1) / s->cfg.retrace_ns);
    if (target < s->cfg.delay_min)
        target = s->cfg.delay_min;
    if (target > s->cfg.delay_cap)
        target = s->cfg.delay_cap;
    int old = s->delay;
    if (target > s->delay) {
        s->delay = target;
        s->good_since = 0;
    } else if (target < s->delay) {
        if (!s->good_since)
            s->good_since = now;
        else if (now - s->good_since >= SHRINK_GOOD_NS && now - s->last_change >= SHRINK_GOOD_NS) {
            s->delay--;
            s->good_since = now;
        }
    } else {
        s->good_since = 0;
    }
    if (s->delay != old) {
        s->last_change = now;
        s->st.delay_changes++;
        if (s->delay < s->st.delay_min_seen)
            s->st.delay_min_seen = s->delay;
        if (s->delay > s->st.delay_max_seen)
            s->st.delay_max_seen = s->delay;
        say(s, 0, "input delay %d -> %d at retrace %u (slowest transit %.1f ms: here %.1f, the peer's report %.1f)",
            old, s->delay, s->submit_next, (need - (int64_t)(s->cfg.delay_margin_ms * 1e6)) / 1e6, local / 1e6,
            s->peer_feedback_have ? s->peer_feedback / 1e6 : -1.0);
    }
}

/* ---------------------------------------------------------------- receiving */

static void rx_seq(NetSession *s, uint32_t seq)
{
    if (!s->rx_any) {
        s->rx_any = 1;
        s->rx_seq_max = s->rx_seq_first = seq;
        s->rx_bits = 1;
        s->rx_unique = 1;
    } else if ((int32_t)(seq - s->rx_seq_max) > 0) {
        uint32_t shift = seq - s->rx_seq_max;
        s->rx_bits = shift >= 64 ? 0 : s->rx_bits << shift;
        s->rx_bits |= 1;
        s->rx_seq_max = seq;
        s->rx_unique++;
    } else {
        uint32_t d = s->rx_seq_max - seq;
        if (d >= 64) {
            s->st.reordered++;
        } else if (s->rx_bits >> d & 1) {
            s->st.duplicates++;
        } else {
            s->rx_bits |= 1ull << d;
            s->rx_unique++;
            s->st.reordered++;
            if ((int32_t)(seq - s->rx_seq_first) < 0)
                s->rx_seq_first = seq;   /* it was sent before the first one to arrive */
        }
    }
}

static void on_bye(NetSession *s, const NetMsg *m)
{
    const NetBye *b = &m->u.bye;
    NetEnd why = (NetEnd)b->reason;
    switch (why) {
    case NET_END_DESYNC:
        check_hashes(s);   /* this side may see it too, at the same retrace */
        end_session(s, NET_END_DESYNC, b->detail, 0, "the two games went out of sync at retrace %u (the peer found it)",
                    b->detail);
        break;
    case NET_END_VERSION:
        end_session(s, NET_END_VERSION, 0, 0, "the %s runs build %s; this is %s: both players need the same build",
                    s->role == NET_HOST ? "guest" : "host", b->text, s->build);
        break;
    case NET_END_MISMATCH:
    case NET_END_REJECTED:
    case NET_END_BUSY:
    case NET_END_PROTOCOL:
        end_session(s, why, b->detail, 0, "the peer ended the session: %s", b->text[0] ? b->text : net_end_name(why));
        break;
    default:
        end_session(s, NET_END_PEER_LEFT, 0, 0, "the other player left the session (%s)", net_end_name(why));
        break;
    }
}

static void refill_tokens(NetSession *s, uint64_t now)
{
    double dt = (double)(now - s->tokens_at) * 1e-9;
    s->tokens_at = now;
    s->hello_tokens += dt * 50;   /* HELLOs handled a second, from everyone: a HELLO_ACK is no bigger than it */
    if (s->hello_tokens > 50)
        s->hello_tokens = 50;
    s->bye_tokens += dt * 5;
    if (s->bye_tokens > 5)
        s->bye_tokens = 5;
}

/* One datagram, as it arrived at now (late: after a gap in pumping, so its arrival time is unknown). Every check a
 * hostile packet could fail comes before anything it says is used. */
static void handle(NetSession *s, const uint8_t *buf, int len, const NetAddr *from, uint64_t now, int late)
{
    NetMsg m;
    int e = net_proto_parse(buf, len, &m);
    s->st.received++;
    s->st.bytes_received += (uint64_t)len;
    refill_tokens(s, now);
    if (e == NET_PARSE_VERSION && m.h.type == NET_MSG_HELLO && s->role == NET_HOST && !s->peer_set &&
        s->hello_tokens >= 1) {
        s->hello_tokens -= 1;
        send_bye_to(s, from, NET_END_VERSION, 0, s->build, 0, now);
        say(s, 1, "a guest with protocol version %u tried to join; this is %u", m.h.version, NET_PROTO_VERSION);
    }
    if (e != NET_PARSE_OK) {
        s->st.rejected++;
        return;
    }
    if (s->keyed ? !(m.has_tag && net_proto_tag_ok(s->key, buf, len)) : m.has_tag) {
        s->st.rejected++;
        s->st.rejected_tag++;
        if (s->st.rejected_tag == 1)
            say(s, 1, s->keyed ? "a packet without this join code's tag: the join codes may differ"
                               : "a packet tagged with a join code, and this side has none");
        return;
    }
    if (s->peer_set ? !net_addr_equal(from, &s->peer)
                    : m.h.type != NET_MSG_HELLO && !(s->role == NET_HOST && confirm_peer(s, &m, from, now))) {
        s->st.rejected++;
        s->st.rejected_source++;
        return;
    }
    if (m.h.sender != (s->role == NET_HOST ? 1 : 0)) {
        s->st.rejected++;
        return;
    }
    if (m.h.type == NET_MSG_HELLO) {
        if (m.h.session != 0) {
            s->st.rejected++;
            return;
        }
    } else if (!s->session_id) {   /* a guest not yet answered: HELLO_ACK, or BYE to its HELLO */
        if (!(m.h.type == NET_MSG_HELLO_ACK || (m.h.type == NET_MSG_BYE && m.h.session == 0))) {
            s->st.rejected++;
            return;
        }
    } else if (m.h.session != s->session_id) {
        s->st.rejected++;
        return;
    }
    if (m.h.type == NET_MSG_HELLO && !s->peer_set) {
        on_hello(s, &m, from, now);   /* the peer's packets are counted once it is the peer */
        return;
    }
    rx_seq(s, m.h.seq);
    s->last_rx = now;
    s->echo_t = m.h.t_send;
    s->echo_rx_at = now;
    s->echo_have = 1;
    if (!late)
        ntp_sample(s, &m.h, now);
    if (s->state == NET_ENDED) {   /* it hasn't heard: say it again (a few a second at most) */
        if (m.h.type != NET_MSG_BYE && s->bye_tokens >= 1 && s->session_id) {
            s->bye_tokens -= 1;
            send_bye_to(s, &s->peer, s->end, s->end_detail, s->end == NET_END_VERSION ? s->build : s->end_msg, 1,
                        now);
        }
        return;
    }
    switch (m.h.type) {
    case NET_MSG_HELLO:
        on_hello(s, &m, from, now);
        break;
    case NET_MSG_HELLO_ACK:
        on_hello_ack(s, &m, now);
        break;
    case NET_MSG_SETTINGS:
    case NET_MSG_FILE:
        if (s->state >= NET_HANDSHAKE)
            on_chunk(s, &m, now);
        break;
    case NET_MSG_FILE_ACK:
        on_chunk_ack(s, &m);
        break;
    case NET_MSG_READY:
        if (s->state < NET_HANDSHAKE)
            break;
        memcpy(s->peer_ready_hash, m.u.ready_hash, NET_HASH_LEN);
        s->peer_ready = 1;
        if (memcmp(s->peer_ready_hash, s->agree, NET_HASH_LEN) != 0)
            end_session(s, NET_END_MISMATCH, 0, 1, "the two sides didn't agree on the build, settings and files");
        break;
    case NET_MSG_START:
        on_start(s, &m, now);
        break;
    case NET_MSG_INPUT:
    case NET_MSG_HASH:
        on_input(s, &m, now, late);
        break;
    case NET_MSG_PING:
        send_ping(s, NET_MSG_PONG, now);
        break;
    case NET_MSG_PONG:
        break;
    case NET_MSG_BYE:
        on_bye(s, &m);
        break;
    }
}

/* ---------------------------------------------------------------- timers and the pump */

static void timers(NetSession *s, uint64_t now)
{
    switch (s->state) {
    case NET_CONNECTING:
        if (s->role == NET_GUEST) {
            if (sdiff(now, s->created) > (int64_t)s->cfg.timeout_ms * (int64_t)MS) {
                char a[80];
                net_addr_str(&s->peer, a, sizeof a);
                end_session(s, NET_END_NO_ANSWER, 0, 0,
                            "no answer from %s in %d s: check the address, the port, the join code and the host's "
                            "firewall",
                            a, s->cfg.timeout_ms / 1000);
                return;
            }
            if (!s->last_hello_tx || now - s->last_hello_tx >= HELLO_NS) {
                send_hello(s, NET_MSG_HELLO, &s->peer, 0, 0, now);
                s->last_hello_tx = now;
            }
        }
        return;
    case NET_HANDSHAKE:
    case NET_ACCEPT:
    case NET_READY:
        if (!s->last_ping_tx || now - s->last_ping_tx >= PING_NS)
            send_ping(s, NET_MSG_PING, now);
        transfer(s, now);
        if (s->state == NET_READY && now - s->last_ready_tx >= RESEND_NS) {
            send_simple(s, NET_MSG_READY, s->agree, NET_HASH_LEN, now);
            s->last_ready_tx = now;
        }
        try_start(s, now);
        break;
    case NET_STARTING:
    case NET_RUNNING:
        transfer(s, now);   /* acks for chunks repeated late */
        if (s->role == NET_HOST && !s->guest_input_seen && now - s->last_start_tx >= RESEND_NS) {
            send_start(s, now);
            s->st.resends++;
        }
        if (now - s->last_input_tx >= (uint64_t)s->cfg.retrace_ns) {   /* keepalive: a retrace without a packet */
            send_input(s, now);
            s->st.keepalives++;
        }
        if (s->need_active && !s->wait_logged && now - s->need_t >= (uint64_t)s->cfg.wait_report_ms * MS) {
            s->wait_logged = 1;
            say(s, 0, "waiting for the peer's input for retrace %u", s->need_n);
        }
        break;
    case NET_ENDED:
        return;
    }
    if (s->peer_set && sdiff(now, s->last_rx) > (int64_t)s->cfg.timeout_ms * (int64_t)MS)
        end_session(s, NET_END_TIMEOUT, 0, 0, "nothing from the other player for %d s: the connection was lost",
                    s->cfg.timeout_ms / 1000);
}

static void pump(NetSession *s)
{
    uint64_t now = now_ns(s);
    NetAddr from;
    int n;
    if (s->sim && net_sim_frozen(s->sim, now)) {   /* stopped: what arrives waits in the socket */
        for (int i = 0; i < 4096 && (n = s->tr.recv(s->tr.ctx, &from, s->rxbuf, (int)sizeof s->rxbuf)) != 0; i++) {
            if (n == -1)
                break;
            if (n > 0)
                net_sim_push(s->sim, now, &from, s->rxbuf, n, 1);
        }
        s->frozen_was = 1;
        return;
    }
    int late = s->frozen_was || (s->last_pump && now - s->last_pump > LATE_PUMP_NS);
    s->frozen_was = 0;
    s->last_pump = now;
    for (int i = 0; i < 4096; i++) {
        n = s->tr.recv(s->tr.ctx, &from, s->rxbuf, (int)sizeof s->rxbuf);
        if (n == 0)
            break;
        if (n < 0) {
            if (n == -2) {
                s->st.received++;
                s->st.rejected++;
                continue;
            }
            break;
        }
        if (s->sim)
            net_sim_push(s->sim, now, &from, s->rxbuf, n, late);
        else
            handle(s, s->rxbuf, n, &from, now, late);
    }
    if (s->sim) {
        int was_late;
        while ((n = net_sim_pop(s->sim, now, &from, s->rxbuf, (int)sizeof s->rxbuf, &was_late)) > 0)
            handle(s, s->rxbuf, n, &from, now, late || was_late);
    }
    timers(s, now);
}

/* How long the service thread (or net_wait) may sleep: until a held packet is due or a timer may be. */
static int64_t next_wait_ns(NetSession *s, uint64_t now)
{
    int64_t w = 10 * (int64_t)MS;
    if (s->sim) {
        uint64_t due = net_sim_next_due(s->sim);
        if (due != UINT64_MAX && sdiff(due, now) < w)
            w = sdiff(due, now);
    }
    if (s->state == NET_STARTING || s->state == NET_RUNNING) {
        int64_t k = sdiff(s->last_input_tx + (uint64_t)s->cfg.retrace_ns, now);
        if (k < w)
            w = k;
    }
    if (w < 200000)
        w = 200000;
    return w;
}

static void *service(void *p)
{
    NetSession *s = p;
    net_mutex_lock(&s->lock);
    while (!s->stop) {
        pump(s);
        net_cond_broadcast(&s->cond);
        int64_t w = next_wait_ns(s, now_ns(s));
        net_mutex_unlock(&s->lock);
        if (s->tr.wait)
            s->tr.wait(s->tr.ctx, (int)(w / 1000));
        else
            net_os_sleep_ns((uint64_t)w);
        net_mutex_lock(&s->lock);
    }
    net_mutex_unlock(&s->lock);
    return NULL;
}

/* ---------------------------------------------------------------- the UDP transport */

static int udp_send(void *ctx, const NetAddr *to, const void *buf, int len) { return net_sock_send(ctx, to, buf, len); }
static int udp_recv(void *ctx, NetAddr *from, void *buf, int cap) { return net_sock_recv(ctx, from, buf, cap); }
static int udp_wait(void *ctx, int timeout_us) { return net_sock_wait(ctx, timeout_us); }

/* ---------------------------------------------------------------- the API */

static char *dupstr(const char *s) { return s ? strdup(s) : NULL; }

NetSession *net_create(const NetConfig *c, char *err, size_t errlen)
{
    char dummy[8];
    if (!err) {
        err = dummy;
        errlen = sizeof dummy;
    }
    err[0] = 0;
    if (!c->ports || (c->ports & 0xF0) || !c->build || strlen(c->build) >= NET_BUILD_LEN || c->nfiles < 0 ||
        c->nfiles > NET_MAX_FILES || c->settings_size > NET_SETTINGS_MAX_SIZE ||
        (c->role == NET_GUEST && !c->transport && !c->peer) || c->retrace_ns <= 0) {
        snprintf(err, errlen, "net: bad configuration");
        return NULL;
    }
    for (int i = 0; i < c->nfiles; i++)
        if (c->files[i].size > NET_FILE_MAX_SIZE || (c->files[i].size && !c->files[i].data)) {
            snprintf(err, errlen, "net: file %d is too large", i);
            return NULL;
        }
    NetSession *s = calloc(1, sizeof *s);
    if (!s) {
        snprintf(err, errlen, "net: out of memory");
        return NULL;
    }
    s->cfg = *c;
    s->build = dupstr(c->build);
    s->peer_str = dupstr(c->peer);
    s->bind_str = dupstr(c->bind);
    s->cfg.build = s->build;
    s->cfg.peer = s->peer_str;
    s->cfg.bind = s->bind_str;
    s->cfg.sim = NULL;
    s->cfg.join_code = NULL;
    s->cfg.settings = NULL;
    s->own_settings = c->settings != NULL;
    if (s->cfg.delay_min < 0)
        s->cfg.delay_min = 0;
    if (s->cfg.delay_cap < s->cfg.delay_min)
        s->cfg.delay_cap = s->cfg.delay_min;
    if (s->cfg.delay_cap > 120)
        s->cfg.delay_cap = 120;
    if (s->cfg.delay_fixed > 120)
        s->cfg.delay_fixed = 120;
    if (s->cfg.timeout_ms <= 0)
        s->cfg.timeout_ms = 15000;
    if (s->cfg.wait_report_ms <= 0)
        s->cfg.wait_report_ms = 250;
    s->role = c->role;
    s->ports = c->ports;
    s->nlocal = net_popcount4(c->ports);
    s->own_sock = 0;
    s->sock.fd = NET_SOCK_INVALID;
    net_mutex_init(&s->lock);
    net_cond_init(&s->cond);
    s->created = s->tokens_at = now_ns(s);
    s->hello_tokens = 50;
    s->bye_tokens = 5;
    s->lslot = calloc(RING, sizeof *s->lslot);
    s->rslot = calloc(RING, sizeof *s->rslot);
    s->lhash = calloc(RING, sizeof *s->lhash);
    s->rhash = calloc(RING, sizeof *s->rhash);
    s->sched = calloc(RING, sizeof *s->sched);
    s->st.end = NET_END_NONE;
    if (!s->lslot || !s->rslot || !s->lhash || !s->rhash || !s->sched || !s->build) {
        snprintf(err, errlen, "net: out of memory");
        net_destroy(s);
        return NULL;
    }
    if (c->join_code && *c->join_code) {
        net_proto_key(c->join_code, s->key);
        s->keyed = 1;
    }
    const char *spec = c->sim ? c->sim : getenv("RUSH_SIM_NET");
    NetSimModel model;
    if (net_sim_parse(spec, &model, err, errlen) != 0) {
        net_destroy(s);
        return NULL;
    }
    if (model.active) {
        char d[400];
        s->sim = net_sim_new(&model, c->seed ? c->seed : 0x5EED0000u + (uint64_t)c->role, s->created);
        net_sim_describe(&model, d, sizeof d);
        say(s, 1, "RUSH_SIM_NET: %s", d);
    }
    /* what this side sends: its HELLO's manifest, the settings (host) and its files */
    NetHello *h = &s->my_hello;
    net_os_random(&s->nonce, sizeof s->nonce);
    h->nonce = s->nonce;
    h->ports = c->ports;
    h->nfiles = (uint8_t)c->nfiles;
    snprintf(h->build, sizeof h->build, "%s", c->build);
    if (c->settings && c->settings_size) {
        h->settings.size = (uint32_t)c->settings_size;
        net_blake2s(h->settings.hash, NET_HASH_LEN, NULL, 0, c->settings, c->settings_size);
        if (c->role == NET_HOST) {
            blob_alloc(&s->my_settings, h->settings.size, h->settings.hash, 1);
            if (s->my_settings.data)
                memcpy(s->my_settings.data, c->settings, c->settings_size);
        }
    }
    for (int i = 0; i < c->nfiles; i++) {
        h->files[i].size = (uint32_t)c->files[i].size;
        if (c->files[i].size)
            net_blake2s(h->files[i].hash, NET_HASH_LEN, NULL, 0, c->files[i].data, c->files[i].size);
        blob_alloc(&s->my_files[i], h->files[i].size, h->files[i].hash, 1);
        if (c->files[i].size && s->my_files[i].data)
            memcpy(s->my_files[i].data, c->files[i].data, c->files[i].size);
    }
    if ((h->settings.size && c->role == NET_HOST && !s->my_settings.data)) {
        snprintf(err, errlen, "net: out of memory");
        net_destroy(s);
        return NULL;
    }
    for (int i = 0; i < c->nfiles; i++)
        if (h->files[i].size && !s->my_files[i].data) {
            snprintf(err, errlen, "net: out of memory");
            net_destroy(s);
            return NULL;
        }
    if (c->transport) {
        s->tr = *c->transport;
        if (c->role == NET_GUEST) {
            s->peer = c->peer_addr;
            s->peer_set = 1;
        }
    } else {
        char host[256];
        int port = 0;
        if (net_sock_startup() != 0) {
            snprintf(err, errlen, "net: the sockets library didn't start");
            net_destroy(s);
            return NULL;
        }
        s->own_sock = 1;
        if (net_sock_open(&s->sock, c->bind, c->role == NET_HOST ? c->port : (c->port > 0 ? c->port : 0), err,
                          errlen) != 0) {
            net_destroy(s);
            return NULL;
        }
        s->tr.ctx = &s->sock;
        s->tr.send = udp_send;
        s->tr.recv = udp_recv;
        s->tr.wait = udp_wait;
        if (c->role == NET_GUEST) {
            if (net_split_hostport(c->peer, host, sizeof host, &port, NET_DEFAULT_PORT) != 0) {
                snprintf(err, errlen, "net: '%s' is not host or host:port", c->peer);
                net_destroy(s);
                return NULL;
            }
            if (net_sock_resolve(&s->sock, host, port, &s->peer, err, errlen) != 0) {
                net_destroy(s);
                return NULL;
            }
            s->peer_set = 1;
        }
    }
    if (c->role == NET_HOST)
        say(s, 0, "listening on port %d%s", net_local_port(s), s->keyed ? " (join code set)" : "");
    else {
        char a[80];
        net_addr_str(&s->peer, a, sizeof a);
        say(s, 0, "connecting to %s%s", a, s->keyed ? " (join code set)" : "");
    }
    s->state = NET_CONNECTING;
    return s;
}

void net_destroy(NetSession *s)
{
    if (!s)
        return;
    net_mutex_lock(&s->lock);
    if (s->state != NET_ENDED && s->state != NET_CONNECTING && s->lslot)
        end_session(s, NET_END_LOCAL, 0, 1, "this side left the session");
    s->stop = 1;
    int joined = s->thread_on;
    net_mutex_unlock(&s->lock);
    if (joined)
        net_thread_join(s->thread);
    if (s->own_sock) {
        net_sock_close(&s->sock);
        net_sock_shutdown();
    }
    net_sim_free(s->sim);
    blob_free(&s->my_settings);
    blob_free(&s->peer_settings);
    for (int i = 0; i < NET_MAX_FILES; i++) {
        blob_free(&s->my_files[i]);
        blob_free(&s->peer_files[i]);
    }
    free(s->lslot);
    free(s->rslot);
    free(s->lhash);
    free(s->rhash);
    free(s->sched);
    free(s->build);
    free(s->peer_str);
    free(s->bind_str);
    net_cond_destroy(&s->cond);
    net_mutex_destroy(&s->lock);
    free(s);
}

int net_start_thread(NetSession *s)
{
    int r = 0;
    net_mutex_lock(&s->lock);
    if (!s->thread_on) {
        r = net_thread_start(&s->thread, service, s);
        if (r == 0)
            s->thread_on = 1;
    }
    net_mutex_unlock(&s->lock);
    return r;
}

void net_poll(NetSession *s)
{
    net_mutex_lock(&s->lock);
    pump(s);
    net_mutex_unlock(&s->lock);
}

void net_wait(NetSession *s, int timeout_us)
{
    net_mutex_lock(&s->lock);
    int64_t w = next_wait_ns(s, now_ns(s));
    if (w > (int64_t)timeout_us * 1000)
        w = (int64_t)timeout_us * 1000;
    net_mutex_unlock(&s->lock);
    if (w > 0) {
        if (s->tr.wait)
            s->tr.wait(s->tr.ctx, (int)(w / 1000));
        else
            net_os_sleep_ns((uint64_t)w);
    }
    net_poll(s);
}

NetState net_state(NetSession *s)
{
    net_mutex_lock(&s->lock);
    NetState st = s->state;
    net_mutex_unlock(&s->lock);
    return st;
}

int net_local_port(NetSession *s) { return s->own_sock ? net_sock_port(&s->sock) : -1; }

int net_peer_settings(NetSession *s, const void **data, size_t *size)
{
    int r = 0;
    net_mutex_lock(&s->lock);
    if (s->role == NET_GUEST && s->state >= NET_ACCEPT && s->peer_settings.complete) {
        *data = s->peer_settings.data;
        *size = s->peer_settings.size;
        r = 1;
    }
    net_mutex_unlock(&s->lock);
    return r;
}

int net_settings_differ(NetSession *s)
{
    net_mutex_lock(&s->lock);
    int r = s->settings_differ;
    net_mutex_unlock(&s->lock);
    return r;
}

int net_peer_files(NetSession *s)
{
    net_mutex_lock(&s->lock);
    int r = s->state >= NET_HANDSHAKE ? s->peer_nfiles : 0;
    net_mutex_unlock(&s->lock);
    return r;
}

int net_peer_file(NetSession *s, int index, const void **data, size_t *size)
{
    int r = 0;
    net_mutex_lock(&s->lock);
    if (s->state >= NET_ACCEPT && index >= 0 && index < s->peer_nfiles && s->peer_files[index].complete) {
        *data = s->peer_files[index].data;
        *size = s->peer_files[index].size;
        r = 1;
    }
    net_mutex_unlock(&s->lock);
    return r;
}

void net_accept(NetSession *s)
{
    net_mutex_lock(&s->lock);
    if (s->state < NET_ACCEPT)
        s->cfg.auto_accept = 1;   /* before it has all arrived: accept it as it does */
    accept_locked(s, now_ns(s));
    net_mutex_unlock(&s->lock);
}

int net_start_time(NetSession *s, uint64_t *local_ns)
{
    net_mutex_lock(&s->lock);
    int r = s->start_known;
    if (r)
        *local_ns = s->start_local;
    net_mutex_unlock(&s->lock);
    return r;
}

static void misuse(NetSession *s, const char *what, uint32_t n)
{
    if (!s->misuse_logged++)
        say(s, 1, "%s(%u) out of order (the caller's error)", what, n);
}

int net_submit(NetSession *s, uint32_t n, const NetPad *pads)
{
    net_mutex_lock(&s->lock);
    if (s->state == NET_ENDED || s->state < NET_STARTING) {
        net_mutex_unlock(&s->lock);
        return -1;
    }
    if (n != s->submit_next) {
        misuse(s, "net_submit", n);
        net_mutex_unlock(&s->lock);
        return -1;
    }
    uint64_t now = now_ns(s);
    Sched *sc = &s->sched[n & RMASK];
    sc->n = n;
    sc->t_start = now;
    sc->t_ready = 0;
    s->state = NET_RUNNING;
    update_delay(s, now);
    uint32_t slot = n + (uint32_t)s->delay;
    if (slot >= s->local_next) {
        while (s->local_next < slot) {   /* the delay grew: the retraces skipped repeat the last input */
            LocalSlot *ls = &s->lslot[s->local_next & RMASK];
            ls->n = s->local_next++;
            ls->t_assign = now;
            memcpy(ls->pad, s->last_assigned, sizeof ls->pad);
            s->st.samples_repeated++;
        }
        LocalSlot *ls = &s->lslot[slot & RMASK];
        ls->n = slot;
        ls->t_assign = now;
        memset(ls->pad, 0, sizeof ls->pad);
        memcpy(ls->pad, pads, (size_t)s->nlocal * sizeof *pads);
        memcpy(s->last_assigned, ls->pad, sizeof s->last_assigned);
        s->local_next = slot + 1;
    } else {
        s->st.samples_dropped++;   /* the delay shrank: this retrace's slot has an input already */
    }
    s->submit_next = n + 1;
    send_input(s, now);
    net_mutex_unlock(&s->lock);
    return 0;
}

static int pads_ready(NetSession *s, uint32_t n, NetPad out[NET_MAX_PORTS], uint64_t now)
{
    const RemoteSlot *r = &s->rslot[n & RMASK];
    if (!(r->have && r->n == n))
        return 0;
    const LocalSlot *l = &s->lslot[n & RMASK];
    int kl = 0, kp = 0;
    for (int p = 0; p < NET_MAX_PORTS; p++) {
        if (s->ports >> p & 1)
            out[p] = l->pad[kl++];
        else if (s->peer_ports >> p & 1)
            out[p] = r->pad[kp++];
        else
            memset(&out[p], 0, sizeof out[p]);
    }
    uint64_t ready = sdiff(r->t_arr, s->need_t) > 0 ? r->t_arr : s->need_t;
    int64_t waited = sdiff(r->t_arr, s->need_t);
    if (waited > (int64_t)STALL_NS) {
        double ms = waited / 1e6;
        s->st.stalls++;
        s->st.stall_ms_total += ms;
        if (ms > s->st.stall_ms_max)
            s->st.stall_ms_max = ms;
        if (s->wait_logged)
            say(s, 0, "the peer's input for retrace %u came after %.0f ms", n, ms);
    }
    Sched *sc = &s->sched[n & RMASK];
    if (sc->n == n)
        sc->t_ready = ready;
    (void)now;
    s->need_active = 0;
    s->consume_next = n + 1;
    return 1;
}

/* After the peer left (BYE), the retraces it ran itself still run here (up to the last it said it submitted), as far
 * as their inputs came: net_linger made sure they had. So this side reaches the retrace the peer left at (a player
 * quitting, a test's run at its limit); before the BYE came it may have run up to the peer's delay past it, on the
 * inputs the peer had assigned that far ahead. The first retrace past them ends it here. */
static int drained(NetSession *s, uint32_t n, NetPad out[NET_MAX_PORTS], uint64_t now)
{
    return s->end == NET_END_PEER_LEFT && s->peer_latest_have && s->peer_cur != NET_RETRACE_NONE &&
           (int32_t)(s->peer_cur - n) >= 0 && n < s->local_next && n >= s->consume_next && pads_ready(s, n, out, now);
}

int net_pads(NetSession *s, uint32_t n, NetPad out[NET_MAX_PORTS], int timeout_ms)
{
    net_mutex_lock(&s->lock);
    uint64_t now = now_ns(s), deadline = now + (uint64_t)(timeout_ms > 0 ? timeout_ms : 0) * MS;
    if (s->state == NET_ENDED) {
        int r = drained(s, n, out, now) ? 1 : -1;
        net_mutex_unlock(&s->lock);
        return r;
    }
    if (s->state < NET_STARTING || n >= s->local_next || n < s->consume_next) {
        misuse(s, "net_pads", n);
        net_mutex_unlock(&s->lock);
        return -1;
    }
    if (!s->need_active || s->need_n != n) {
        s->need_active = 1;
        s->need_n = n;
        s->need_t = now;
        s->wait_logged = 0;
    }
    for (;;) {
        if (s->state == NET_ENDED) {
            int r = drained(s, n, out, now) ? 1 : -1;
            net_mutex_unlock(&s->lock);
            return r;
        }
        if (pads_ready(s, n, out, now)) {
            net_mutex_unlock(&s->lock);
            return 1;
        }
        if (!s->thread_on) {
            pump(s);
            if (s->state != NET_ENDED && pads_ready(s, n, out, now)) {
                net_mutex_unlock(&s->lock);
                return 1;
            }
        }
        now = now_ns(s);
        if (sdiff(deadline, now) <= 0)
            break;
        int64_t w = sdiff(deadline, now);
        if (s->thread_on) {
            net_cond_wait_ns(&s->cond, &s->lock, (uint64_t)w);
        } else {
            int64_t t = next_wait_ns(s, now);
            if (t < w)
                w = t;
            net_mutex_unlock(&s->lock);
            if (s->tr.wait)
                s->tr.wait(s->tr.ctx, (int)(w / 1000));
            else
                net_os_sleep_ns((uint64_t)w);
            net_mutex_lock(&s->lock);
        }
        now = now_ns(s);
    }
    net_mutex_unlock(&s->lock);
    return 0;
}

int net_waiting_ms(NetSession *s)
{
    net_mutex_lock(&s->lock);
    int ms = s->need_active ? (int)(sdiff(now_ns(s), s->need_t) / (int64_t)MS) : 0;
    net_mutex_unlock(&s->lock);
    return ms < 0 ? 0 : ms;
}

void net_hash(NetSession *s, uint32_t n, uint32_t hash)
{
    net_mutex_lock(&s->lock);
    if (n != s->lhash_next || n >= s->consume_next) {
        misuse(s, "net_hash", n);
    } else if (s->state != NET_ENDED) {
        if (s->lhash_next - s->hash_checked >= RING - 16)   /* the peer gives no hashes: keep the newest */
            s->hash_checked = s->lhash_next - (RING - 16);
        HashSlot *x = &s->lhash[n & RMASK];
        x->n = n;
        x->h = hash;
        x->have = 1;
        s->lhash_next = n + 1;
        check_hashes(s);
    }
    net_mutex_unlock(&s->lock);
}

void net_window_hash(NetSession *s, uint32_t n, uint64_t hash)
{
    net_mutex_lock(&s->lock);
    if (s->state != NET_ENDED) {
        WHash *x = &s->lwh[n % WHASH_RING];
        x->n = n;
        x->h = hash;
        x->have = 1;
        check_whash(s, n);
    }
    net_mutex_unlock(&s->lock);
}

/* Time sync (docs/port/NETWORK.md 5): when this side's next retrace would begin before the peer's (by the peer's
 * last report of when it began a retrace, moved to this side's clock by the offset and on by whole retraces), this
 * side waits half the difference, at most 1 ms a retrace; never less than 0: the side behind doesn't race. */
int64_t net_pace_adjust_ns(NetSession *s)
{
    int64_t adj = 0;
    net_mutex_lock(&s->lock);
    uint32_t n = s->submit_next - 1;
    const Sched *sc = &s->sched[n & RMASK];
    if (s->submit_next && sc->n == n && sc->t_ready && s->peer_latest_have && s->peer_cur != NET_RETRACE_NONE &&
        s->theta_valid && s->state == NET_RUNNING) {
        int64_t p = s->cfg.retrace_ns;
        uint64_t next = sdiff(sc->t_ready, sc->t_start) > p / 2 ? sc->t_ready + (uint64_t)p   /* re-based */
                                                                : sc->t_start + (uint64_t)p;
        uint64_t peer_next =
            s->peer_cur_start - (uint64_t)s->theta + (uint64_t)((int64_t)(int32_t)(n + 1 - s->peer_cur) * p);
        /* Each sample carries both sides' wake-up jitter (a retrace's start is when its submit came): smoothed over
         * about 8 retraces, and this side's own adjustment taken off the estimate as it is made, since it shortens
         * the lead at once (so the smoothing doesn't overshoot). */
        int64_t sample = sdiff(peer_next, next);
        if (!s->adv_have) {
            s->adv = sample;
            s->adv_have = 1;
        } else {
            s->adv += (sample - s->adv) / 8;
        }
        if (s->adv > (int64_t)SYNC_DEADBAND_NS) {
            adj = s->adv / 2;
            if (adj > (int64_t)SYNC_MAX_NS)
                adj = SYNC_MAX_NS;
            s->adv -= adj;
            s->st.sync_corrections++;
            s->st.sync_ms_total += adj / 1e6;
        }
        s->st.advantage_ms = s->adv / 1e6;
    }
    net_mutex_unlock(&s->lock);
    return adj;
}

int net_delay(NetSession *s)
{
    net_mutex_lock(&s->lock);
    int d = s->delay;
    net_mutex_unlock(&s->lock);
    return d;
}

int net_frozen(NetSession *s)
{
    net_mutex_lock(&s->lock);
    int f = s->sim && net_sim_frozen(s->sim, now_ns(s));
    net_mutex_unlock(&s->lock);
    return f;
}

void net_end(NetSession *s, NetEnd reason)
{
    net_end_text(s, reason, NULL);
}

void net_end_text(NetSession *s, NetEnd reason, const char *text)
{
    net_mutex_lock(&s->lock);
    if (reason == NET_END_NONE)
        reason = NET_END_LOCAL;
    end_session(s, reason, 0, 1, "%s", text && *text                ? text
                                     : reason == NET_END_LOCAL    ? "this side left the session"
                                     : reason == NET_END_REJECTED ? "the settings or files were rejected"
                                                                  : net_end_name(reason));
    net_mutex_unlock(&s->lock);
}

int net_linger(NetSession *s, int timeout_ms)
{
    net_mutex_lock(&s->lock);
    uint64_t now = now_ns(s), deadline = now + (uint64_t)(timeout_ms > 0 ? timeout_ms : 0) * MS;
    int r = 0;
    for (;;) {
        if (s->state != NET_RUNNING && s->state != NET_STARTING)
            break;
        if ((int32_t)(s->peer_ack - s->local_next) >= 0) {
            r = 1;
            break;
        }
        now = now_ns(s);
        int64_t w = sdiff(deadline, now);
        if (w <= 0)
            break;
        if (w > 5 * (int64_t)MS)
            w = 5 * (int64_t)MS;
        if (s->thread_on) {
            net_cond_wait_ns(&s->cond, &s->lock, (uint64_t)w);
        } else {
            pump(s);
            net_mutex_unlock(&s->lock);
            if (s->tr.wait)
                s->tr.wait(s->tr.ctx, (int)(w / 1000));
            else
                net_os_sleep_ns((uint64_t)w);
            net_mutex_lock(&s->lock);
        }
    }
    net_mutex_unlock(&s->lock);
    return r;
}

NetEnd net_end_reason(NetSession *s, uint32_t *detail, char *msg, size_t n)
{
    net_mutex_lock(&s->lock);
    NetEnd e = s->state == NET_ENDED ? s->end : NET_END_NONE;
    if (detail)
        *detail = s->end_detail;
    if (msg && n)
        snprintf(msg, n, "%s", e ? s->end_msg : "");
    net_mutex_unlock(&s->lock);
    return e;
}

void net_stats(NetSession *s, NetStats *st)
{
    net_mutex_lock(&s->lock);
    uint64_t now = now_ns(s);
    *st = s->st;
    st->state = s->state;
    st->end = s->state == NET_ENDED ? s->end : NET_END_NONE;
    st->end_detail = s->end_detail;
    st->retrace = s->submit_next ? s->submit_next - 1 : NET_NONE;
    st->peer_retrace = s->peer_latest_have ? s->peer_cur : NET_NONE;
    st->rtt_ms = s->rtt_avg;
    st->rtt_min_ms = s->rtt_min_all / 1e6;
    st->rtt_max_ms = s->rtt_max_all / 1e6;
    st->jitter_ms = s->rtt_jit;
    st->clock_offset_ms = s->theta / 1e6;
    st->delay = s->delay;
    st->peer_delay = s->peer_delay;
    int64_t t = window_max(&s->transit, now, TRANSIT_WINDOW_NS);
    st->transit_ms = t >= 0 ? t / 1e6 : -1;
    st->feedback_ms = s->peer_feedback_have ? s->peer_feedback / 1e6 : -1;
    uint64_t span = s->rx_any ? (uint64_t)(s->rx_seq_max - s->rx_seq_first) + 1 : 0;
    st->lost = span > s->rx_unique ? span - s->rx_unique : 0;
    if (s->sim) {
        NetSimStats ss;
        net_sim_stats(s->sim, &ss);
        st->sim_dropped = ss.dropped;
        st->sim_duplicated = ss.duplicated;
        st->sim_reordered = ss.reordered;
    }
    net_mutex_unlock(&s->lock);
}

void net_stats_line(const NetStats *st, char *buf, size_t n)
{
    snprintf(buf, n,
             "retrace %d rtt %.1f ms (min %.1f max %.1f jitter %.1f) delay %d (peer %d, %d-%d, %d changes) transit "
             "%.1f/%.1f ms stalls %llu (%.0f ms, max %.0f) sync %llu (%.1f ms) sent %llu recv %llu lost %llu dup "
             "%llu reord %llu resends %llu rejected %llu hashes %llu",
             st->retrace == NET_NONE ? -1 : (int)st->retrace, st->rtt_ms, st->rtt_min_ms, st->rtt_max_ms,
             st->jitter_ms, st->delay, st->peer_delay, st->delay_min_seen, st->delay_max_seen, st->delay_changes,
             st->transit_ms, st->feedback_ms, (unsigned long long)st->stalls, st->stall_ms_total, st->stall_ms_max,
             (unsigned long long)st->sync_corrections, st->sync_ms_total, (unsigned long long)st->sent,
             (unsigned long long)st->received, (unsigned long long)st->lost, (unsigned long long)st->duplicates,
             (unsigned long long)st->reordered, (unsigned long long)st->resends, (unsigned long long)st->rejected,
             (unsigned long long)st->hash_checks);
}

void net_test_inject(NetSession *s, const void *buf, int len, const NetAddr *from)
{
    net_mutex_lock(&s->lock);
    if (len > 0 && len <= (int)sizeof s->rxbuf) {
        uint8_t copy[2048];
        memcpy(copy, buf, (size_t)len);
        handle(s, copy, len, from, now_ns(s), 0);
    }
    net_mutex_unlock(&s->lock);
}
