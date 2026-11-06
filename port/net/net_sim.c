/* port/net/net_sim.c: the RUSH_SIM_NET model (net_sim.h): a min-heap of held datagrams by release time. */
#include "net_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIM_MAX_HELD 16384

typedef struct Held {
    uint64_t due, order;
    NetAddr from;
    int len, flag;
    uint8_t *data;
} Held;

struct NetSim {
    NetSimModel m;
    uint64_t origin, rng, order;
    Held *heap;
    int n, cap;
    NetSimStats st;
};

static int parse_num(const char *s, double *out)
{
    char *e;
    double v = strtod(s, &e);
    if (e == s || *e || v < 0 || v != v)
        return -1;
    *out = v;
    return 0;
}

/* "a:b" or "a:b:c" */
static int parse_tuple(const char *s, double *v, int n)
{
    char buf[64];
    if (strlen(s) >= sizeof buf)
        return -1;
    strcpy(buf, s);
    char *p = buf;
    for (int i = 0; i < n; i++) {
        char *c = i < n - 1 ? strchr(p, ':') : NULL;
        if (i < n - 1 && !c)
            return -1;
        if (c)
            *c = 0;
        if (parse_num(p, &v[i]) != 0)
            return -1;
        p = c ? c + 1 : p;
    }
    return 0;
}

int net_sim_parse(const char *spec, NetSimModel *m, char *err, size_t errlen)
{
    char buf[1024], dummy[8];
    if (!err) {
        err = dummy;
        errlen = sizeof dummy;
    }
    memset(m, 0, sizeof *m);
    m->nphases = 1;
    if (!spec || !*spec)
        return 0;
    if (strlen(spec) >= sizeof buf) {
        snprintf(err, errlen, "RUSH_SIM_NET: too long");
        return -1;
    }
    strcpy(buf, spec);
    NetSimPhase *ph = &m->phase[0];
    for (char *tok = buf, *next; tok; tok = next) {   /* comma-separated (no strtok: sessions start in threads) */
        next = strchr(tok, ',');
        if (next)
            *next++ = 0;
        while (*tok == ' ')
            tok++;
        if (!*tok)
            continue;
        char *eq = strchr(tok, '=');
        if (!eq) {
            snprintf(err, errlen, "RUSH_SIM_NET: '%s' is not key=value", tok);
            return -1;
        }
        *eq = 0;
        const char *k = tok, *v = eq + 1;
        double x = 0, t[3];
        int bad = 0;
        if (!strcmp(k, "delay"))
            bad = parse_num(v, &ph->delay_ms);
        else if (!strcmp(k, "jitter"))
            bad = parse_num(v, &ph->jitter_ms);
        else if (!strcmp(k, "loss"))
            bad = parse_num(v, &ph->loss_pct) || ph->loss_pct > 100;
        else if (!strcmp(k, "dup"))
            bad = parse_num(v, &ph->dup_pct) || ph->dup_pct > 100;
        else if (!strcmp(k, "reorder"))
            bad = parse_num(v, &ph->reorder_pct) || ph->reorder_pct > 100;
        else if (!strcmp(k, "at")) {
            bad = parse_num(v, &x) || m->nphases >= NET_SIM_PHASES || x < ph->at_s;
            if (!bad) {   /* a new phase: the same values from x on, until changed */
                m->phase[m->nphases] = *ph;
                ph = &m->phase[m->nphases++];
                ph->at_s = x;
            }
        } else if (!strcmp(k, "burst")) {
            bad = parse_tuple(v, t, 3) || t[0] <= 0 || t[2] > 100;
            if (!bad) {
                m->burst_every_s = t[0];
                m->burst_len_s = t[1];
                m->burst_loss_pct = t[2];
            }
        } else if (!strcmp(k, "spike")) {
            bad = parse_tuple(v, t, 3) || t[0] <= 0;
            if (!bad) {
                m->spike_every_s = t[0];
                m->spike_len_s = t[1];
                m->spike_ms = t[2];
            }
        } else if (!strcmp(k, "pause")) {
            bad = parse_tuple(v, t, 2);
            if (!bad) {
                m->pause_at_s = t[0];
                m->pause_len_s = t[1];
            }
        } else if (!strcmp(k, "seed")) {
            char *e;
            m->seed = strtoull(v, &e, 0);
            m->has_seed = 1;
            bad = e == v || *e;
        } else {
            snprintf(err, errlen, "RUSH_SIM_NET: unknown key '%s'", k);
            return -1;
        }
        if (bad) {
            snprintf(err, errlen, "RUSH_SIM_NET: bad value for %s: '%s'", k, v);
            return -1;
        }
    }
    m->active = 1;
    return 0;
}

void net_sim_describe(const NetSimModel *m, char *buf, size_t n)
{
    size_t at = 0;
    if (!m->active) {
        snprintf(buf, n, "none");
        return;
    }
    buf[0] = 0;
    for (int i = 0; i < m->nphases && at < n; i++) {
        const NetSimPhase *p = &m->phase[i];
        at += (size_t)snprintf(buf + at, n - at, "%sfrom %.1fs: %.0f ms ± %.0f, loss %.1f%%, dup %.1f%%, reorder %.1f%%",
                               i ? "; " : "", p->at_s, p->delay_ms, p->jitter_ms, p->loss_pct, p->dup_pct,
                               p->reorder_pct);
    }
    if (m->burst_every_s > 0 && at < n)
        at += (size_t)snprintf(buf + at, n - at, "; bursts of %.0f%% loss for %.2fs every %.2fs", m->burst_loss_pct,
                               m->burst_len_s, m->burst_every_s);
    if (m->spike_every_s > 0 && at < n)
        at += (size_t)snprintf(buf + at, n - at, "; spikes of +%.0f ms for %.2fs every %.2fs", m->spike_ms,
                               m->spike_len_s, m->spike_every_s);
    if (m->pause_len_s > 0 && at < n)
        snprintf(buf + at, n - at, "; a pause at %.1fs for %.1fs", m->pause_at_s, m->pause_len_s);
}

NetSim *net_sim_new(const NetSimModel *m, uint64_t seed, uint64_t origin_ns)
{
    NetSim *s = calloc(1, sizeof *s);
    if (!s)
        return NULL;
    s->m = *m;
    s->origin = origin_ns;
    s->rng = (m->has_seed ? m->seed : seed) * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull;
    if (!s->rng)
        s->rng = 1;
    return s;
}

void net_sim_free(NetSim *s)
{
    if (!s)
        return;
    for (int i = 0; i < s->n; i++)
        free(s->heap[i].data);
    free(s->heap);
    free(s);
}

static uint64_t next_u64(NetSim *s)   /* xorshift64* */
{
    s->rng ^= s->rng >> 12;
    s->rng ^= s->rng << 25;
    s->rng ^= s->rng >> 27;
    return s->rng * 0x2545F4914F6CDD1Dull;
}

static double uniform(NetSim *s) { return (double)(next_u64(s) >> 11) * (1.0 / 9007199254740992.0); }

static int chance(NetSim *s, double pct) { return pct > 0 && uniform(s) * 100.0 < pct; }

static double secs(const NetSim *s, uint64_t now) { return now > s->origin ? (double)(now - s->origin) * 1e-9 : 0; }

static uint64_t pause_end(const NetSim *s)
{
    return s->origin + (uint64_t)((s->m.pause_at_s + s->m.pause_len_s) * 1e9);
}

int net_sim_frozen(const NetSim *s, uint64_t now)
{
    if (!s || s->m.pause_len_s <= 0)
        return 0;
    double t = secs(s, now);
    return t >= s->m.pause_at_s && t < s->m.pause_at_s + s->m.pause_len_s;
}

static int in_window(double t, double every, double len)
{
    if (every <= 0 || len <= 0)
        return 0;
    double k = t / every;
    return t - (double)(uint64_t)k * every < len;
}

static int before(const Held *a, const Held *b) { return a->due < b->due || (a->due == b->due && a->order < b->order); }

static void heap_push(NetSim *s, uint64_t due, const NetAddr *from, const void *buf, int len, int flag)
{
    if (s->n >= SIM_MAX_HELD) {
        s->st.overflow++;
        return;
    }
    if (s->n == s->cap) {
        int nc = s->cap ? s->cap * 2 : 256;
        Held *h = realloc(s->heap, (size_t)nc * sizeof *h);
        if (!h) {
            s->st.overflow++;
            return;
        }
        s->heap = h;
        s->cap = nc;
    }
    Held e = {due, s->order++, *from, len, flag, malloc((size_t)len)};
    if (!e.data) {
        s->st.overflow++;
        return;
    }
    memcpy(e.data, buf, (size_t)len);
    int i = s->n++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (!before(&e, &s->heap[p]))
            break;
        s->heap[i] = s->heap[p];
        i = p;
    }
    s->heap[i] = e;
}

void net_sim_push(NetSim *s, uint64_t now, const NetAddr *from, const void *buf, int len, int flag)
{
    double t = secs(s, now);
    const NetSimPhase *ph = &s->m.phase[0];
    for (int i = 1; i < s->m.nphases; i++)
        if (t >= s->m.phase[i].at_s)
            ph = &s->m.phase[i];
    double loss = ph->loss_pct;
    if (in_window(t, s->m.burst_every_s, s->m.burst_len_s) && s->m.burst_loss_pct > loss)
        loss = s->m.burst_loss_pct;
    s->st.in++;
    if (chance(s, loss)) {
        s->st.dropped++;
        return;
    }
    int copies = chance(s, ph->dup_pct) ? 2 : 1;
    if (copies == 2)
        s->st.duplicated++;
    for (int c = 0; c < copies; c++) {
        double d = ph->delay_ms + ph->jitter_ms * (2 * uniform(s) - 1);
        if (in_window(t, s->m.spike_every_s, s->m.spike_len_s))
            d += s->m.spike_ms;
        if (chance(s, ph->reorder_pct)) {
            d += 5 + 35 * uniform(s);
            s->st.reordered++;
        }
        if (d < 0)
            d = 0;
        uint64_t due = now + (uint64_t)(d * 1e6);
        if (net_sim_frozen(s, now) && due < pause_end(s))
            due = pause_end(s);   /* a stopped process reads it when it runs again */
        heap_push(s, due, from, buf, len, flag);
    }
}

int net_sim_pop(NetSim *s, uint64_t now, NetAddr *from, void *buf, int cap, int *flag)
{
    if (!s->n || s->heap[0].due > now || net_sim_frozen(s, now))
        return 0;
    Held top = s->heap[0], last = s->heap[--s->n];
    int i = 0;
    for (;;) {   /* sift the last entry down from the root */
        int l = 2 * i + 1, r = l + 1, m = i;
        const Held *best = &last;
        if (l < s->n && before(&s->heap[l], best)) {
            m = l;
            best = &s->heap[l];
        }
        if (r < s->n && before(&s->heap[r], best))
            m = r;
        if (m == i)
            break;
        s->heap[i] = s->heap[m];
        i = m;
    }
    if (s->n)
        s->heap[i] = last;
    int len = top.len < cap ? top.len : cap;
    memcpy(buf, top.data, (size_t)len);
    *from = top.from;
    if (flag)
        *flag = top.flag;
    free(top.data);
    return len;
}

uint64_t net_sim_next_due(const NetSim *s) { return s->n ? s->heap[0].due : UINT64_MAX; }

void net_sim_stats(const NetSim *s, NetSimStats *st)
{
    if (s)
        *st = s->st;
    else
        memset(st, 0, sizeof *st);
}
