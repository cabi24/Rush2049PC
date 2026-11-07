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
 * The addresses and the MagicDNS name are netaddr.c's (the F1 Online page lists them too); the name is looked up on
 * a thread of its own, since DNS may wait: the screen shows at once and the name joins it.
 *
 * Under them, the router's lines (netrouter.c, docs/port/NETPLAY.md 16): U opens the port on this PC's router
 * (net.upnp, off by default) or closes it again, and the lines say "Your friend joins at A.B.C.D:PORT" or why it can't
 * work there; each line takes an address row's room.
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
#include "netaddr.h"
#include "netmenu.h"
#include "netplay.h"
#include "netrouter.h"
#include "ultra.h"

#define SHOWN 6                  /* address rows on the screen; the log lists them all */
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
    int on, port, code;
    PcNetAddrs a;                     /* the addresses, best first; the screen shows the first SHOWN */
    char status[160];                 /* "A player is joining from ..." once one is */
    uint64_t elapsed_ns;
    int seen;                         /* the lookup's state as the last drawing had it */
    int cancel, dirty, shot_done, said;
    int k, ox, oy;
} W;

/* ---------------------------------------------------------------- the addresses */

static void find_addresses(void)
{
    pc_net_addrs_find(&W.a);   /* netaddr.c: best first, logged; the MagicDNS name on a thread */
}

#define rank pc_net_addrs_rank
#define tag pc_net_addrs_tag

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
    char line[192], ip[64], router[PC_NET_ROUTER_LINES][PC_NET_ROUTER_LINE];
    int y, shown = 0, tone[PC_NET_ROUTER_LINES];
    static const uint32_t tones[] = {C_HINT, C_OK, C_WARN, C_VALUE};
    int nrouter = pc_net_router_lines(router, tone);   /* netrouter.c: net.upnp's lines (NETPLAY.md 16) */
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
    int lookup = W.seen = pc_net_addrs_lookup(&W.a);
    for (int i = 0; i < W.a.naddr && shown < SHOWN - nrouter && rank(&W.a.addr[W.a.order[i]]) < 90; i++, shown++) {
        const NetLocalAddr *a = &W.a.addr[W.a.order[i]];
        net_addr_ip(&a->addr, ip, sizeof ip);
        text(18, y, C_VALUE, ip);
        text_right(y, C_HINT, tag(a));
        y += ROW;
        if (W.a.ts_any && net_addr_equal(&a->addr, &W.a.ts4) && lookup) {   /* its MagicDNS name, under it */
            if (lookup == 2 && W.a.magic[0]) {
                snprintf(line, sizeof line, "%.56s", W.a.magic);
                text(18, y, C_VALUE, line);
                text_right(y, C_HINT, "Tailscale name");
            } else {
                text(18, y, C_HINT, lookup == 1 ? "(its Tailscale name: looking it up...)"
                                                : "(no Tailscale name found: MagicDNS off?)");
            }
            y += ROW;
        }
    }
    if (W.a.naddr > shown) {
        snprintf(line, sizeof line, "(and %d more: the log lists them)", W.a.naddr - shown);
        text(18, y, C_HINT, line);
        y += ROW;
    }
    if (!W.a.naddr) {
        text(18, y, C_WARN, "None found: is this PC connected to a network?");
        y += ROW;
    } else if (!W.a.ts_any) {
        text(18, y, C_HINT, "(no Tailscale address here: is Tailscale running?)");
        y += ROW;
    }
    y += 4;
    text(10, y, C_HINT, "They start \"Join online race\" and type one of them in.");
    y += ROW + 8;
    for (int i = 0; i < nrouter; i++, y += ROW)   /* the router: U opens the port, or what it said */
        text(10, y, tones[tone[i]], router[i]);
    if (nrouter)
        y += 4;
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
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_u && !e->key.repeat) {   /* net.upnp (netrouter.c) */
        pc_net_router_toggle();
        W.dirty = 1;
    }
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

/* The window was closed before the game (not Esc): the player stopped the program (netmenu.c's return). */
static int g_closed;

int pc_net_window_closed(void)
{
    return g_closed;
}

int pc_net_screen_pump(void)
{
    if (pc_gfx_poll_events())   /* the window closed */
        W.cancel = g_closed = 1;
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
        if (W.a.magic[0])
            fprintf(stderr, "rush2049pc: online: this PC's Tailscale name (MagicDNS): %s\n", W.a.magic);
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
    pc_net_addrs_close(&W.a);
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
        return g_closed = 1;
    n = SDL_PeepEvents(ev, 64, SDL_PEEKEVENT, SDL_WINDOWEVENT, SDL_WINDOWEVENT);
    for (int i = 0; i < n; i++)
        if (ev[i].window.event == SDL_WINDOWEVENT_CLOSE)
            return g_closed = 1;
    n = SDL_PeepEvents(ev, 64, SDL_PEEKEVENT, SDL_KEYDOWN, SDL_KEYDOWN);
    for (int i = 0; i < n; i++)
        if (ev[i].key.keysym.sym == SDLK_ESCAPE)
            return 1;
#endif
    return 0;
}
