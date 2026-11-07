/* port/app/netaddr.c: this PC's addresses a friend can join with, best first, and the MagicDNS name of its Tailscale
 * address (netmenu.h), for the host's waiting screen (netwait.c) and the F1 Online page (netmenu.c) alike.
 *
 * The MagicDNS name is asked of Tailscale's own resolver (100.100.100.100) as the PTR of this PC's 100.x address,
 * then of the system's resolver (getnameinfo), then taken from the computer's canonical name; only a name under
 * ts.net counts. On a thread of its own, since DNS may wait. Nothing of Tailscale's own program is used. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "netaddr.h"

#define LOOKUP_MS 1500           /* Tailscale's resolver's answer */

static int is_ts_name(char *s)
{
    size_t n = strlen(s);
    if (n && s[n - 1] == '.')
        s[--n] = 0;
    return (n > 7 && !strcasecmp(s + n - 7, ".ts.net")) || (n > 14 && !strcasecmp(s + n - 14, ".tailscale.net"));
}

static void *lookup(void *arg)
{
    PcNetAddrs *a = arg;
    char name[256] = "", host[256];
    int ok;
    ok = net_dns_ptr("100.100.100.100", &a->ts4, LOOKUP_MS, name, sizeof name) == 0 && is_ts_name(name);
    if (!ok)
        ok = net_addr_name(&a->ts4, name, sizeof name) == 0 && is_ts_name(name);
    if (!ok)
        ok = net_host_names(host, sizeof host, name, sizeof name) == 0 && is_ts_name(name);
    if (ok)
        snprintf(a->magic, sizeof a->magic, "%s", name);
    __atomic_store_n(&a->lookup, 2, __ATOMIC_RELEASE);
    return NULL;
}

/* Interfaces no friend reaches: containers' and virtual machines' bridges (Linux names them). */
static int bridge(const char *ifname)
{
    static const char *const pre[] = {"docker", "br-", "veth", "virbr", "vboxnet", "vmnet", "podman", "cni", "lxc",
                                      "lxd", "flannel", "kube"};
    for (size_t i = 0; i < sizeof pre / sizeof pre[0]; i++)
        if (!strncmp(ifname, pre[i], strlen(pre[i])))
            return 1;
    return 0;
}

static int tailscale(const NetLocalAddr *a)
{
    return net_addr_kind(&a->addr) == NET_KIND_TAILSCALE &&
           (!a->ifname[0] || !strncmp(a->ifname, "tailscale", 9) || !strncmp(a->ifname, "utun", 4));
}

/* The order shown: Tailscale's (IPv4 first), the local network's (192.168, 10, then 172.16-31, which containers and
 * WSL often take), public IPv4, public IPv6, the rest; bridges last. */
int pc_net_addrs_rank(const NetLocalAddr *a)
{
    int k = net_addr_kind(&a->addr), v6 = a->addr.family == NET_AF_INET6;
    if (bridge(a->ifname))
        return 90;
    if (tailscale(a))
        return v6;
    if (k == NET_KIND_LAN && !v6)
        return a->addr.addr[0] == 192 ? 10 : a->addr.addr[0] == 10 ? 11 : 12;
    if (k == NET_KIND_PUBLIC)
        return v6 ? 30 : 20;
    return v6 ? 41 : 40;
}

const char *pc_net_addrs_tag(const NetLocalAddr *a)
{
    int k = net_addr_kind(&a->addr);
    if (tailscale(a))
        return "Tailscale";
    if (k == NET_KIND_TAILSCALE)
        return "shared range";   /* 100.64/10 on another interface: a carrier's NAT, not Tailscale */
    if (k == NET_KIND_LAN)
        return bridge(a->ifname) ? "virtual" : "local network";
    if (k == NET_KIND_PUBLIC)
        return a->addr.family == NET_AF_INET6 ? "internet, IPv6" : "internet";
    return "";
}

void pc_net_addrs_find(PcNetAddrs *a)
{
    char ip[64], line[2048];
    size_t len = 0;
    memset(a, 0, sizeof *a);
    net_sock_startup();   /* Windows' sockets library (a session has it started already: counted) */
    a->naddr = net_local_addrs(a->addr, PC_NET_ADDRS);
    for (int i = 0; i < a->naddr; i++)
        a->order[i] = i;
    for (int i = 1; i < a->naddr; i++)   /* stable: the system's order within a rank */
        for (int j = i; j > 0 && pc_net_addrs_rank(&a->addr[a->order[j]]) < pc_net_addrs_rank(&a->addr[a->order[j - 1]]);
             j--) {
            int t = a->order[j];
            a->order[j] = a->order[j - 1], a->order[j - 1] = t;
        }
    line[0] = 0;
    for (int i = 0; i < a->naddr; i++) {
        const NetLocalAddr *x = &a->addr[a->order[i]];
        net_addr_ip(&x->addr, ip, sizeof ip);
        if (len < sizeof line)
            len += (size_t)snprintf(line + len, sizeof line - len, "%s%s (%s%s%s)", i ? ", " : "", ip,
                                    pc_net_addrs_tag(x)[0] ? pc_net_addrs_tag(x) : "other", x->ifname[0] ? ", " : "",
                                    x->ifname);
        if (tailscale(x) && x->addr.family == NET_AF_INET && !a->ts_any) {
            a->ts4 = x->addr;
            a->ts_any = 1;
        }
    }
    fprintf(stderr, "rush2049pc: online: this PC's addresses: %s\n", a->naddr ? line : "none found");
    if (a->ts_any) {
        __atomic_store_n(&a->lookup, 1, __ATOMIC_RELAXED);
        if (net_thread_start(&a->thread, lookup, a) == 0)
            a->thread_on = 1;
        else
            __atomic_store_n(&a->lookup, 2, __ATOMIC_RELEASE);
    }
}

int pc_net_addrs_lookup(const PcNetAddrs *a)
{
    return __atomic_load_n(&a->lookup, __ATOMIC_ACQUIRE);
}

void pc_net_addrs_close(PcNetAddrs *a)
{
    if (a->thread_on && pc_net_addrs_lookup(a) == 2) {
        net_thread_join(a->thread);   /* done; one still waiting on DNS is left to end by itself */
        a->thread_on = 0;
    }
}
