/* port/net/tests/net_peer.c: one peer of a two-process session around the toy simulation, for
 * tools/port/net_check.py (docs/port/NETWORK.md 8): the host prints "port N" once it listens; both print
 * "tick N" every 60 ticks, then a "result" line, and write each tick's hash to --hashes.
 *
 *   net_peer --host [--port P] [--bind ADDR] | --join HOST[:PORT]
 *            [--ticks N] [--sim SPEC] [--seed S] [--delay N] [--desync-at T] [--quit-at T] [--hashes FILE]
 *            [--code CODE] [--timeout MS] [--files N] [--linger MS] [--build B]
 *            [--window MS] [--recover] [--rejoin SESSION]
 *
 * Rejoins (docs/port/NETWORK.md 12): both print "session S" (hex) once the session has started; --rejoin S is a new
 * game of a side whose game was killed, which replays the other's log and goes on; --recover rebuilds after a
 * desync (the guest's game ends "recover", and the test starts its rejoin); --window the silence a running session
 * waits through.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver.h"
#include "net.h"
#include "net_os.h"
#include "toy.h"

#ifdef _WIN32
#include <mmsystem.h>
#endif

#define MS 1000000ull

static void log_line(void *user, const char *line)
{
    (void)user;
    printf("log %s\n", line);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    NetConfig c;
    char err[300];
    const char *hashes = NULL;
    uint32_t ticks = 600;
    int64_t desync_at = -1, quit_at = -1;
    int linger_ms = 1000, nfiles = 0;
    uint64_t seed = 0;
    static uint8_t files[NET_FILES_MAX][32768];
#ifdef _WIN32
    setvbuf(stdout, NULL, _IONBF, 0);   /* msvcrt's _IOLBF is full buffering: net_check.py reads lines as they come */
#else
    setvbuf(stdout, NULL, _IOLBF, 0);
#endif
#ifdef _WIN32
    timeBeginPeriod(1);   /* the service thread's select() timeouts, as the game asks for (docs/port/WINDOWS.md 5) */
#endif
    net_config_defaults(&c);
    c.build = "net_peer";
    c.port = 0;
    c.auto_accept = 1;
    c.log = log_line;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
#define ARG(name) (!strcmp(a, name) && v && (i++, 1))
        if (!strcmp(a, "--host"))
            c.role = NET_HOST, c.ports = 0x1;
        else if (ARG("--join"))
            c.role = NET_GUEST, c.ports = 0x2, c.peer = v;
        else if (ARG("--port"))
            c.port = atoi(v);
        else if (ARG("--bind"))
            c.bind = v;
        else if (ARG("--ticks"))
            ticks = (uint32_t)atol(v);
        else if (ARG("--sim"))
            c.sim = v;
        else if (ARG("--seed"))
            seed = strtoull(v, NULL, 0);
        else if (ARG("--delay"))
            c.delay_fixed = atoi(v);
        else if (ARG("--desync-at"))
            desync_at = atol(v);
        else if (ARG("--quit-at"))
            quit_at = atol(v);
        else if (ARG("--hashes"))
            hashes = v;
        else if (ARG("--code"))
            c.join_code = v;
        else if (ARG("--timeout"))
            c.timeout_ms = atoi(v);
        else if (ARG("--files"))
            nfiles = atoi(v);
        else if (ARG("--linger"))
            linger_ms = atoi(v);
        else if (ARG("--build"))
            c.build = v;
        else if (ARG("--window"))
            c.window_ms = atoi(v);
        else if (!strcmp(a, "--recover"))
            c.recover = 1;
        else if (ARG("--rejoin"))
            c.rejoin_session = strtoull(v, NULL, 16);
        else {
            fprintf(stderr, "net_peer: unknown or incomplete argument %s\n", a);
            return 2;
        }
    }
    if (nfiles > NET_FILES_MAX)
        nfiles = NET_FILES_MAX;
    for (int i = 0; i < nfiles; i++) {
        for (int k = 0; k < 32768; k++)
            files[i][k] = (uint8_t)(k * 31 + i * 7 + c.role * 3);
        c.files[i].data = files[i];
        c.files[i].size = sizeof files[i];
    }
    c.nfiles = nfiles;
    c.seed = seed + (uint64_t)c.role + 1;
    NetSession *s = net_create(&c, err, sizeof err);
    if (!s) {
        printf("error %s\n", err);
        return 1;
    }
    if (c.role == NET_HOST)
        printf("port %d\n", net_local_port(s));
    net_start_thread(s);
    Driver d;
    drv_init(&d, s, c.role == NET_HOST ? 0 : 1, ticks, (c.role == NET_HOST ? 0x1111 : 0x2222) ^ seed, 1000000000 / 60,
             1000 * MS);
    d.desync_at = desync_at;
    d.quit_at = quit_at;
    int r, said = 0;
    while ((r = drv_tick(&d)) > 0) {
        if (!said && net_session_id(s)) {
            printf("session %016llx\n", (unsigned long long)net_session_id(s));
            said = 1;
        }
        if (d.n % 60 == 0)
            printf("tick %u\n", d.n);
    }
    uint64_t t_end = net_os_now_ns();
    /* linger: the peer's last hashes, and BYE answered */
    while (net_os_now_ns() - t_end < (uint64_t)linger_ms * MS && net_state(s) != NET_ENDED)
        net_os_sleep_ns(5 * MS);
    if (hashes) {
        FILE *f = fopen(hashes, "w");
        if (f) {
            for (uint32_t i = 0; i < d.n; i++)
                fprintf(f, "%u %08x\n", i, d.hashes[i]);
            fclose(f);
        }
    }
    NetStats st;
    uint32_t detail;
    char msg[200], line[700], trace[1000];
    net_end_reason(s, &detail, msg, sizeof msg);
    net_stats(s, &st);
    net_stats_line(&st, line, sizeof line);
    drv_trace_str(&d, trace, sizeof trace);
    double rate = drv_rate(&d, d.n / 3, d.n ? d.n - 1 : 0);
    printf("stats %s\n", line);
    printf("result end=%s detail=%u ticks=%u rate=%.3f stalls=%llu stall_ms=%.0f stall_max=%.0f delay_min=%d "
           "delay_max=%d changes=%d checks=%llu wchecks=%llu sync=%llu rejected=%llu rejoins=%u recoveries=%u "
           "replayed=%u replay_ms=%.1f outage=%llu trace=%s\n",
           net_end_name(st.end), detail, d.n, rate, (unsigned long long)st.stalls, st.stall_ms_total,
           st.stall_ms_max, st.delay_min_seen, st.delay_max_seen, st.delay_changes,
           (unsigned long long)st.hash_checks, (unsigned long long)st.window_hash_checks,
           (unsigned long long)st.sync_corrections, (unsigned long long)st.rejected, st.rejoins, st.recoveries,
           d.replayed, d.replay_ns / 1e6, (unsigned long long)st.outage_inputs, trace[0] ? trace : "-");
    if (st.end)
        printf("ended %s\n", msg);
    net_destroy(s);   /* BYE if still running */
    drv_free(&d);
    return 0;
}
