/* port/app/netwait.c: the host's waiting screen (docs/port/NETPLAY.md 12.10), and the window's events while a
 * session gets ready before the game.
 *
 * The host adopts nothing of its guest's, so it opens its window (and the renderer) before a guest joins, and until
 * one has, this screen fills it: the port, this PC's addresses the friend can join with (Tailscale's first, with its
 * MagicDNS name, then the local network's and the public ones), the join code, Windows' firewall, "Esc cancels". It
 * is drawn with the overlay's font (pc_gfx_ui_text) over a cleared frame (pc_gfx_redraw before the game has
 * presented one), and its events come through the renderer's event hook: port/input isn't set up yet (main's
 * pc_input_init comes after it), so no hotkey or settings overlay can change a setting the guest is being sent.
 *
 * The MagicDNS name is asked of Tailscale's own resolver (100.100.100.100) as the PTR of this PC's 100.x address,
 * then of the system's resolver (getnameinfo), then taken from the computer's canonical name; only a name under
 * ts.net counts. On a thread of its own, since DNS may wait: the screen shows at once and the name joins it. Nothing
 * of Tailscale's own program is used.
 *
 * RUSH2049_TEST_NET_WAIT_SHOT=PATH (tests): once the lookups are done (or after 5 s), the window as drawn is saved
 * to PATH (pc_gfx_save_window), then the screen is cancelled as Escape would. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifdef PC_HAVE_SDL2
#include <SDL.h>
#endif

#include "app.h"
#include "net.h"
#include "net_os.h"
#include "netplay.h"
#include "ultra.h"

#define MAX_ADDRS 32
#define SHOWN 6                  /* address rows on the screen; the log lists them all */
#define LOOKUP_MS 1500           /* Tailscale's resolver's answer */
#define SHOT_AFTER_NS 5000000000u

/* The panel, in units: a unit is the largest whole number of pixels that fits it (and a margin) in the window. */
#define UW 408
#define UH 236
#define ROW 11
#define C_PANEL 0x0C1018FFu
#define C_TITLE 0x1C2C4CFFu
#define C_EDGE 0x4A6EA8FFu
#define C_WHITE 0xFFFFFFFFu
#define C_VALUE 0x8CC8FFFFu
#define C_HINT 0x9098A8FFu
#define C_OK 0x7CE07CFFu
#define C_WARN 0xFFA050FFu

static struct {
    int on, port, code, ts_any;
    NetLocalAddr addr[MAX_ADDRS];
    int naddr, order[MAX_ADDRS];      /* the addresses, best first; the screen shows the first SHOWN */
    NetAddr ts4;                      /* the first Tailscale IPv4 address, whose name is looked up */
    int lookup;                       /* 0 none, 1 running, 2 done (atomic: the thread's result is published) */
    char magic[256];                  /* the MagicDNS name, "" if none was found */
    NetThread thread;
    int thread_on;
    char status[160];                 /* "A player is joining from ..." once one is */
    uint64_t elapsed_ns;
    int seen;                         /* the lookup's state as the last drawing had it */
    int cancel, dirty, shot_done, said;
    int k, ox, oy;
} W;

/* ---------------------------------------------------------------- the addresses */

static int is_ts_name(char *s)
{
    size_t n = strlen(s);
    if (n && s[n - 1] == '.')
        s[--n] = 0;
    return (n > 7 && !strcasecmp(s + n - 7, ".ts.net")) || (n > 14 && !strcasecmp(s + n - 14, ".tailscale.net"));
}

static void *lookup(void *arg)
{
    char name[256] = "", host[256];
    int ok;
    (void)arg;
    ok = net_dns_ptr("100.100.100.100", &W.ts4, LOOKUP_MS, name, sizeof name) == 0 && is_ts_name(name);
    if (!ok)
        ok = net_addr_name(&W.ts4, name, sizeof name) == 0 && is_ts_name(name);
    if (!ok)
        ok = net_host_names(host, sizeof host, name, sizeof name) == 0 && is_ts_name(name);
    if (ok)
        snprintf(W.magic, sizeof W.magic, "%s", name);
    __atomic_store_n(&W.lookup, 2, __ATOMIC_RELEASE);
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
static int rank(const NetLocalAddr *a)
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

static const char *tag(const NetLocalAddr *a)
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

static void find_addresses(void)
{
    char ip[64], line[2048];
    size_t len = 0;
    W.naddr = net_local_addrs(W.addr, MAX_ADDRS);
    for (int i = 0; i < W.naddr; i++)
        W.order[i] = i;
    for (int i = 1; i < W.naddr; i++)   /* stable: the system's order within a rank */
        for (int j = i; j > 0 && rank(&W.addr[W.order[j]]) < rank(&W.addr[W.order[j - 1]]); j--) {
            int t = W.order[j];
            W.order[j] = W.order[j - 1], W.order[j - 1] = t;
        }
    line[0] = 0;
    for (int i = 0; i < W.naddr; i++) {
        const NetLocalAddr *a = &W.addr[W.order[i]];
        net_addr_ip(&a->addr, ip, sizeof ip);
        if (len < sizeof line)
            len += (size_t)snprintf(line + len, sizeof line - len, "%s%s (%s%s%s)", i ? ", " : "", ip,
                                    tag(a)[0] ? tag(a) : "other", a->ifname[0] ? ", " : "", a->ifname);
        if (tailscale(a) && a->addr.family == NET_AF_INET && !W.ts_any) {
            W.ts4 = a->addr;
            W.ts_any = 1;
        }
    }
    fprintf(stderr, "rush2049pc: online: this PC's addresses: %s\n", W.naddr ? line : "none found");
    if (W.ts_any) {
        __atomic_store_n(&W.lookup, 1, __ATOMIC_RELAXED);
        if (net_thread_start(&W.thread, lookup, NULL) == 0)
            W.thread_on = 1;
        else
            __atomic_store_n(&W.lookup, 2, __ATOMIC_RELEASE);
    }
}

/* ---------------------------------------------------------------- the screen */

static void rect(int x, int y, int w, int h, uint32_t color)
{
    pc_gfx_ui_rect(W.ox + x * W.k, W.oy + y * W.k, w * W.k, h * W.k, color);
}

static void text(int x, int y, uint32_t color, const char *s)
{
    pc_gfx_ui_text(W.ox + x * W.k, W.oy + y * W.k, W.k, color, s);
}

static void text_right(int y, uint32_t color, const char *s)
{
    text(UW - 10 - (int)strlen(s) * PC_GFX_FONT_ADVANCE, y, color, s);
}

static void draw(int w, int h)
{
    char line[192], ip[64];
    int y, shown = 0;
    W.k = w / (UW + 12) < h / (UH + 4) ? w / (UW + 12) : h / (UH + 4);
    if (W.k < 1)
        W.k = 1;
    W.ox = (w - UW * W.k) / 2;
    W.oy = (h - UH * W.k) / 2;
    rect(0, 0, UW, UH, C_PANEL);
    rect(0, 0, UW, 14, C_TITLE);
    rect(0, 0, UW, 1, C_EDGE);
    rect(0, UH - 1, UW, 1, C_EDGE);
    rect(0, 0, 1, UH, C_EDGE);
    rect(UW - 1, 0, 1, UH, C_EDGE);
    text(6, 3, C_WHITE, "RUSH 2049 PC  -  ONLINE RACE");
    text_right(3, C_HINT, "host");

    y = 22;
    snprintf(line, sizeof line, "Hosting an online race on port %d.", W.port);
    text(10, y, C_WHITE, line);
    y += ROW;
    if (W.status[0]) {
        text(10, y, C_OK, W.status);
    } else {
        unsigned s = (unsigned)(W.elapsed_ns / 1000000000u);
        snprintf(line, sizeof line, "Waiting for a player to join...  %u:%02u", s / 60, s % 60);
        text(10, y, C_WHITE, line);
    }
    y += ROW + 8;

    text(10, y, C_HINT, "Your friend joins with one of this PC's addresses:");
    y += ROW + 3;
    int lookup = W.seen = __atomic_load_n(&W.lookup, __ATOMIC_ACQUIRE);
    for (int i = 0; i < W.naddr && shown < SHOWN && rank(&W.addr[W.order[i]]) < 90; i++, shown++) {
        const NetLocalAddr *a = &W.addr[W.order[i]];
        net_addr_ip(&a->addr, ip, sizeof ip);
        text(18, y, C_VALUE, ip);
        text_right(y, C_HINT, tag(a));
        y += ROW;
        if (W.ts_any && net_addr_equal(&a->addr, &W.ts4) && lookup) {   /* its MagicDNS name, under it */
            if (lookup == 2 && W.magic[0]) {
                snprintf(line, sizeof line, "%.56s", W.magic);
                text(18, y, C_VALUE, line);
                text_right(y, C_HINT, "Tailscale name");
            } else {
                text(18, y, C_HINT, lookup == 1 ? "(its Tailscale name: looking it up...)"
                                                : "(no Tailscale name found: MagicDNS off?)");
            }
            y += ROW;
        }
    }
    if (W.naddr > shown) {
        snprintf(line, sizeof line, "(and %d more: the log lists them)", W.naddr - shown);
        text(18, y, C_HINT, line);
        y += ROW;
    }
    if (!W.naddr) {
        text(18, y, C_WARN, "None found: is this PC connected to a network?");
        y += ROW;
    } else if (!W.ts_any) {
        text(18, y, C_HINT, "(no Tailscale address here: is Tailscale running?)");
        y += ROW;
    }
    y += 4;
    text(10, y, C_HINT, "They start \"Join online race\" and type one of them in.");
    y += ROW + 8;
    if (W.port != NET_DEFAULT_PORT) {
        snprintf(line, sizeof line, "Not the usual port: your friend adds :%d after the address.", W.port);
        text(10, y, C_WHITE, line);
        y += ROW;
    }
    if (W.code) {
        text(10, y, C_WHITE, "A join code is set: your friend gives the same code.");
        y += ROW;
    }
#ifdef _WIN32
    text(10, y, C_HINT, "If Windows asks, allow the program on private networks.");
    y += ROW;
#endif
    (void)y;
    text(10, UH - 16, C_WHITE, "Esc cancels.");
}

static int on_event(const void *ev)
{
#ifdef PC_HAVE_SDL2
    const SDL_Event *e = (const SDL_Event *)ev;
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE && !e->key.repeat)
        W.cancel = 1;
    if (e->type == SDL_WINDOWEVENT)
        W.dirty = 1;   /* exposed, resized: drawn again at once */
#else
    (void)ev;
#endif
    return 0;
}

void pc_net_screen_open(int port, int code)
{
    memset(&W, 0, sizeof W);
    W.on = 1;
    W.port = port;
    W.code = code;
    W.dirty = 1;
    find_addresses();
    pc_gfx_set_ui_hook(draw);
    pc_gfx_set_event_hook(on_event);   /* -1 headless: nothing to hear */
#ifdef PC_HAVE_SDL2
    if (SDL_WasInit(SDL_INIT_VIDEO) && SDL_GL_GetCurrentWindow()) {
        SDL_SetWindowTitle(SDL_GL_GetCurrentWindow(), "San Francisco Rush 2049 - hosting an online race");
        SDL_RaiseWindow(SDL_GL_GetCurrentWindow());   /* its keys, not the console's it was started from */
    }
#endif
}

void pc_net_screen_status(const char *status)
{
    if (strcmp(W.status, status ? status : "")) {
        snprintf(W.status, sizeof W.status, "%s", status ? status : "");
        W.dirty = 1;
    }
}

int pc_net_screen_pump(void)
{
    if (pc_gfx_poll_events())   /* the window closed */
        W.cancel = 1;
    return W.cancel;
}

int pc_net_screen_dirty(void)
{
    return W.dirty;
}

void pc_net_screen_draw(uint64_t elapsed_ns)
{
    const char *shot = getenv("RUSH2049_TEST_NET_WAIT_SHOT");
    W.elapsed_ns = elapsed_ns;
    W.dirty = 0;
    pc_gfx_redraw();
    if (W.seen == 2 && !W.said) {   /* the lookup's result, once */
        W.said = 1;
        if (W.magic[0])
            fprintf(stderr, "rush2049pc: online: this PC's Tailscale name (MagicDNS): %s\n", W.magic);
        else
            fprintf(stderr, "rush2049pc: online: no Tailscale name found for this PC (MagicDNS off?)\n");
    }
    if (shot && *shot && !W.shot_done && (W.seen != 1 || elapsed_ns > SHOT_AFTER_NS)) {   /* as drawn */
        W.shot_done = 1;
        if (pc_gfx_save_window(shot) == 0)
            fprintf(stderr, "rush2049pc: online: test: the waiting screen saved to %s; cancelled as Escape would\n",
                    shot);
        else
            fprintf(stderr, "rush2049pc: online: test: the waiting screen couldn't be saved to %s\n", shot);
        W.cancel = 1;
    }
}

void pc_net_screen_close(void)
{
    if (!W.on)
        return;
    W.on = 0;
    pc_gfx_set_ui_hook(NULL);
    pc_gfx_set_event_hook(NULL);
    if (W.thread_on && __atomic_load_n(&W.lookup, __ATOMIC_ACQUIRE) == 2) {
        net_thread_join(W.thread);   /* done; one still waiting on DNS is left to end by itself */
        W.thread_on = 0;
    }
}

/* ---------------------------------------------------------------- before the game, after port/input's setup */

int pc_net_events_leave(void)
{
#ifdef PC_HAVE_SDL2
    SDL_Event ev[64];
    int n;
    if (!SDL_WasInit(SDL_INIT_VIDEO) || !SDL_GL_GetCurrentWindow())
        return 0;
    /* The events stay in the queue for the game's loop (keys held, a resize): only looked at here */
    SDL_PumpEvents();
    if (SDL_PeepEvents(ev, 1, SDL_PEEKEVENT, SDL_QUIT, SDL_QUIT) > 0)
        return 1;
    n = SDL_PeepEvents(ev, 64, SDL_PEEKEVENT, SDL_WINDOWEVENT, SDL_WINDOWEVENT);
    for (int i = 0; i < n; i++)
        if (ev[i].window.event == SDL_WINDOWEVENT_CLOSE)
            return 1;
    n = SDL_PeepEvents(ev, 64, SDL_PEEKEVENT, SDL_KEYDOWN, SDL_KEYDOWN);
    for (int i = 0; i < n; i++)
        if (ev[i].key.keysym.sym == SDLK_ESCAPE)
            return 1;
#endif
    return 0;
}
