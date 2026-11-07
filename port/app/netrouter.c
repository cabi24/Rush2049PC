/* port/app/netrouter.c: the host's router asked to open the session's UDP port (net.upnp, off by default;
 * docs/port/NETPLAY.md 16), and what the host is told about it: the address a friend joins at, or a sentence saying
 * why forwarding can't work here and that Tailscale does. The asking, the lease and its renewals, and the removal are
 * port/net/net_nat.c's, on a thread of its own; this side only starts it, reads its status and stops it.
 *
 * Off, nothing is sent to the router or anywhere else. On (the setting, U on the waiting screen, F1 > Online > Host
 * a race), the host asks once its port is open (pc_net_connect); the mapping is removed when the session ends, or left
 * to its lease (10 minutes) when this PC's game goes and is to come back (a rejoin: BYE gone). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "net.h"
#include "net_nat.h"
#include "net_sock.h"
#include "netrouter.h"

#define STOP_MS 2500   /* how long the end waits for the router to take the mapping back */

static struct {
    NetNat *nat;
    int port, on, started;
    char settings[PC_CFG_PATH];
    char said[PC_NET_ROUTER_LINES][PC_NET_ROUTER_LINE];   /* the sentences the log has */
} R;

static void nat_log(void *user, const char *line)
{
    (void)user;
    fprintf(stderr, "rush2049pc: online: %s\n", line);
}

static void ask(void)
{
    NetNatConfig c;
    net_nat_config_defaults(&c);
    c.internal_port = R.port;
    c.log = nat_log;
    R.nat = net_nat_start(&c);
    if (!R.nat)
        fprintf(stderr, "rush2049pc: online: router: can't ask it (no thread)\n");
}

/* A run that stops on an error before its session's end (no renderer, memory that can't be mapped) exits through
 * exit(): the mapping is removed there too. The game's own end is _exit, after pc_net_finish. */
static void at_exit(void)
{
    if (R.nat)
        pc_net_router_finish(0);
}

void pc_net_router_start(int port, int on, const char *settings)
{
    static int registered;
    if (!registered && atexit(at_exit) == 0)
        registered = 1;
    memset(&R, 0, sizeof R);
    R.port = port;
    R.on = on;
    R.started = 1;
    snprintf(R.settings, sizeof R.settings, "%s", settings ? settings : "");
    if (on)
        ask();
    else
        fprintf(stderr, "rush2049pc: online: router: not asked to open the port (net.upnp = off; U on the waiting "
                "screen asks it)\n");
}

void pc_net_router_toggle(void)
{
    char why[PC_CFG_PATH + 64];
    PcSettingValue ch;
    if (!R.started)
        return;
    R.on = !R.on;
    fprintf(stderr, "rush2049pc: online: router: U: %s\n", R.on ? "open the port on my router" : "close it again");
    if (R.on) {
        ask();
    } else {
        net_nat_stop(R.nat, 0, STOP_MS);
        R.nat = NULL;
    }
    memset(R.said, 0, sizeof R.said);
    if (!R.settings[0])
        return;
    ch = (PcSettingValue){"net", "upnp", R.on ? "on" : "off"};
    if (pc_settings_save(R.settings, &ch, 1, why, sizeof why) != 0)
        fprintf(stderr, "rush2049pc: online: router: net.upnp not kept: %s\n", why);
    else
        fprintf(stderr, "rush2049pc: online: router: net.upnp = %s kept in %s\n", R.on ? "on" : "off", R.settings);
}

int pc_net_router_working(void)
{
    NetNatStatus st;
    if (!R.nat)
        return 0;
    net_nat_status(R.nat, &st);
    return st.state == NET_NAT_WORKING;
}

int pc_net_router_external(char *out, size_t n, char *ip, size_t ip_n, int *port)
{
    NetNatStatus st;
    char a[64];
    if (!R.nat)
        return -1;
    net_nat_status(R.nat, &st);
    if (st.state != NET_NAT_MAPPED || st.why != NET_NAT_OK || st.external.family != NET_AF_INET)
        return -1;
    net_addr_ip(&st.external, a, sizeof a);
    if (n)
        snprintf(out, n, "%s:%u", a, st.external.port);
    if (ip_n)
        snprintf(ip, ip_n, "%s", a);
    if (port)
        *port = st.external.port;
    return 0;
}

int pc_net_router_lines(char out[][PC_NET_ROUTER_LINE], int *tone)
{
    NetNatStatus st;
    char a[64] = "", detail[48];
    int n = 0, p = R.port;
    const char *proto;
    if (!R.started)
        return 0;
#define LINE(t, ...) (snprintf(out[n], PC_NET_ROUTER_LINE, __VA_ARGS__), tone[n++] = (t))
    if (!R.on) {
        LINE(PC_NET_ROUTER_HINT, "Not on Tailscale? U: Open the port on my router (UPnP).");
    } else {
        net_nat_status(R.nat, &st);
        proto = net_nat_proto_name(st.proto);
        if (st.external.family)
            net_addr_ip(&st.external, a, sizeof a);
        snprintf(detail, sizeof detail, "%.47s", st.detail);
        switch (st.state) {
        case NET_NAT_WORKING:
            LINE(PC_NET_ROUTER_HINT, "Asking the router to open port %d (UPnP, NAT-PMP, PCP)...", p);
            break;
        case NET_NAT_MAPPED:
            if (st.why == NET_NAT_DOUBLE) {
                LINE(PC_NET_ROUTER_WARN, "Router: port %d open (%s), but behind another router", st.external.port,
                     proto);
                LINE(PC_NET_ROUTER_WARN, "(its address %s): forward there too, or Tailscale.", a);
            } else if (st.external.family) {
                LINE(PC_NET_ROUTER_VALUE, "Your friend joins at %s:%u", a, st.external.port);
                LINE(PC_NET_ROUTER_OK, "Router: port open (%s) while hosting. U closes it.", proto);
            } else {
                LINE(PC_NET_ROUTER_OK, "Router: port %u open (%s). U closes it.", st.external.port, proto);
                LINE(PC_NET_ROUTER_HINT, "Your friend joins at your internet address, port %u.", st.external.port);
            }
            break;
        case NET_NAT_FAILED:
            switch (st.why) {
            case NET_NAT_REFUSED:
                LINE(PC_NET_ROUTER_WARN, "The router refused: %.44s", detail);
                LINE(PC_NET_ROUTER_WARN, "Turn on UPnP in it, forward UDP %d by hand, or use Tailscale.", p);
                break;
            case NET_NAT_CONFLICT:
                LINE(PC_NET_ROUTER_WARN, "The router forwards port %d to another PC already.", p);
                LINE(PC_NET_ROUTER_WARN, "Host on another port (F1 > Online), or use Tailscale.");
                break;
            case NET_NAT_NO_INTERNET:
                LINE(PC_NET_ROUTER_WARN, "The router has no internet address now: is it online?");
                LINE(PC_NET_ROUTER_WARN, "Try again when it is, or use Tailscale.");
                break;
            case NET_NAT_CGNAT:
                LINE(PC_NET_ROUTER_WARN, "Your provider shares one address (CGNAT, %s):", a);
                LINE(PC_NET_ROUTER_WARN, "no port can be opened for you on it. Use Tailscale.");
                break;
            case NET_NAT_LOST:
                LINE(PC_NET_ROUTER_WARN, "The router stopped renewing port %d: it may be closed.", p);
                LINE(PC_NET_ROUTER_WARN, "Press U twice to ask again, or use Tailscale.");
                break;
            default:
                LINE(PC_NET_ROUTER_WARN, "No router answered (UPnP, NAT-PMP, PCP): is UPnP on in it?");
                LINE(PC_NET_ROUTER_WARN, "Forward UDP port %d to this PC by hand, or use Tailscale.", p);
                break;
            }
            break;
        default:
            LINE(PC_NET_ROUTER_WARN, "Couldn't ask the router (no thread for it).");
            break;
        }
    }
#undef LINE
    int changed = 0;
    for (int i = 0; i < PC_NET_ROUTER_LINES; i++)
        changed |= strcmp(R.said[i], i < n ? out[i] : "") != 0;
    if (changed) {
        for (int i = 0; i < PC_NET_ROUTER_LINES; i++)
            snprintf(R.said[i], sizeof R.said[i], "%s", i < n ? out[i] : "");
        fprintf(stderr, "rush2049pc: online: router: the waiting screen says: %s%s%s\n", out[0], n > 1 ? " / " : "",
                n > 1 ? out[1] : "");
    }
    return n;
}

void pc_net_router_finish(int keep)
{
    if (R.nat)
        net_nat_stop(R.nat, keep, STOP_MS);
    R.nat = NULL;
    R.started = 0;
}
