/* port/input/input.c: the pads the SI reports (pc_input_poll, once per retrace; docs/port/FRONTEND.md).
 *
 * Each retrace: live input (the keyboard and game controllers, input_sdl.c, in SDL2 builds with a window),
 * then scripted presses OR'ed over it, then the rumble pak's motor passed to the controllers. Controller 1 is
 * always connected; another port is when a controller is assigned to it or a script uses it.
 *
 * Scripted presses use v5's format, so the same scripts drive both builds (docs/PORT_PLAN.md Phase 7):
 *     --press T:BUTTON[@PORT][:DUR],...       T and DUR in seconds since boot (DUR 0.2 by default)
 *     --press-file PATH                       one `TIME BUTTON[@PORT] [DURATION]` per line, `#` comments,
 *                                             `pak on|off`, `frames N`
 * BUTTON: A B Z START UP DOWN LEFT RIGHT L R CU CD CL CR STICKUP STICKDOWN STICKLEFT STICKRIGHT. The ports a
 * script uses are connected. A scripted stick direction replaces that axis of the live stick. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input_internal.h"
#include "ultra.h"

typedef struct {
    double t, dur;
    int port;
    uint16_t mask;
    int8_t x, y;
} Press;

static Press *g_press;
static int g_npress, g_max_port;
int pc_input_script_pak = -1;          /* `pak on|off` in a press file: -1 none */
uint32_t pc_input_script_frames;       /* `frames N` */

static int parse_button(const char *b, Press *p)
{
    static const struct { const char *name; uint16_t mask; } table[] = {
        {"A", 0x8000}, {"B", 0x4000}, {"Z", 0x2000}, {"START", 0x1000}, {"UP", 0x0800}, {"DOWN", 0x0400},
        {"LEFT", 0x0200}, {"RIGHT", 0x0100}, {"L", 0x0020}, {"R", 0x0010}, {"CU", 0x0008}, {"CD", 0x0004},
        {"CL", 0x0002}, {"CR", 0x0001}};
    for (size_t i = 0; i < sizeof table / sizeof *table; i++)
        if (!strcmp(b, table[i].name)) {
            p->mask = table[i].mask;
            return 0;
        }
    if (!strcmp(b, "STICKUP"))
        p->y = 80;
    else if (!strcmp(b, "STICKDOWN"))
        p->y = -80;
    else if (!strcmp(b, "STICKLEFT"))
        p->x = -80;
    else if (!strcmp(b, "STICKRIGHT"))
        p->x = 80;
    else
        return -1;
    return 0;
}

/* One item: T BUTTON[@PORT] [DUR]; sep is ':' on the command line, whitespace in files. */
static int add_press(char *item, const char *sep)
{
    char *save = NULL, *t = strtok_r(item, sep, &save), *b = strtok_r(NULL, sep, &save),
         *d = strtok_r(NULL, sep, &save);
    if (!t || !b)
        return -1;
    Press p = {atof(t), d ? atof(d) : 0.2, 0, 0, 0, 0};
    char *at = strchr(b, '@');
    if (at) {
        *at = 0;
        p.port = atoi(at + 1) - 1;
        if (p.port < 0 || p.port > 3)
            return -1;
    }
    if (parse_button(b, &p) != 0)
        return -1;
    g_press = realloc(g_press, (size_t)(g_npress + 1) * sizeof *g_press);
    g_press[g_npress++] = p;
    if (p.port > g_max_port)
        g_max_port = p.port;
    return 0;
}

int pc_input_add_spec(const char *spec)
{
    char *copy = strdup(spec), *save = NULL;
    for (char *item = strtok_r(copy, ",", &save); item; item = strtok_r(NULL, ",", &save)) {
        char buf[128];
        snprintf(buf, sizeof buf, "%s", item);
        if (add_press(buf, ":") != 0) {
            fprintf(stderr, "rush2049pc: bad --press item '%s' (T:BUTTON[@PORT][:DUR])\n", item);
            free(copy);
            return -1;
        }
    }
    free(copy);
    return 0;
}

int pc_input_add_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "rush2049pc: cannot read %s\n", path);
        return -1;
    }
    char line[256];
    int n = 0, err = 0;
    while (fgets(line, sizeof line, f)) {
        n++;
        char *hash = strchr(line, '#');
        if (hash)
            *hash = 0;
        char word[32], value[32];
        if (sscanf(line, "%31s", word) != 1)
            continue;
        if (!strcmp(word, "frames") && sscanf(line, "%*s %u", &pc_input_script_frames) == 1)
            continue;
        if (!strcmp(word, "pak") && sscanf(line, "%*s %31s", value) == 1) {
            if (!strcmp(value, "on") || !strcmp(value, "off"))
                pc_input_script_pak = !strcmp(value, "on");
            continue;   /* `pak from`, `pak fail`: v5 harness lines */
        }
        if (add_press(line, " \t\r\n") != 0) {
            fprintf(stderr, "rush2049pc: %s:%d: bad line\n", path, n);
            err = -1;
        }
    }
    fclose(f);
    return err;
}

/* ---------------------------------------------------------------- rumble and the input log */

/* The motor is on or off, and a game may pulse it for a weaker effect: the strength is 1 while it is on and
 * otherwise the share of the last 4 retraces it was on, so a pulsed motor gives a steady, weaker rumble. */
#define RUMBLE_WINDOW 4
static uint8_t g_motor[4][RUMBLE_WINDOW];
static int g_motor_pos;
static double g_rumble_strength = 1.0;

static void rumble(void)
{
    g_motor_pos = (g_motor_pos + 1) % RUMBLE_WINDOW;
    for (int port = 0; port < 4; port++) {
        int on = pc_si_motor(port), n = 0;
        g_motor[port][g_motor_pos] = (uint8_t)on;
        for (int i = 0; i < RUMBLE_WINDOW; i++)
            n += g_motor[port][i];
        pc_input_live_rumble(port, g_rumble_strength * (on ? 1.0 : (double)n / RUMBLE_WINDOW));
    }
}

int pc_input_log;
static PcPad g_logged[4];

static void log_changes(const PcPad pads[4])
{
    static const char *const names[16] = {"C-RIGHT", "C-LEFT", "C-DOWN", "C-UP", "R", "L", "?", "?",
                                          "D-RIGHT", "D-LEFT", "D-DOWN", "D-UP", "START", "Z", "B", "A"};
    for (int port = 0; port < 4; port++) {
        const PcPad *p = &pads[port], *o = &g_logged[port];
        if (p->buttons == o->buttons && p->stick_x == o->stick_x && p->stick_y == o->stick_y &&
            p->connected == o->connected)
            continue;
        char list[128] = "";
        for (int bit = 15; bit >= 0; bit--)
            if (p->buttons & (1u << bit))
                snprintf(list + strlen(list), sizeof list - strlen(list), "%s%s", *list ? "+" : "", names[bit]);
        fprintf(stderr, "rush2049pc: input player %d: %s stick x=%+d y=%+d%s\n", port + 1, *list ? list : "-",
                p->stick_x, p->stick_y, p->connected ? "" : " (not connected)");
        g_logged[port] = *p;
    }
}

/* ---------------------------------------------------------------- the interface */

int pc_input_init(const PcSettings *s)
{
    g_rumble_strength = s->rumble;
    return pc_input_live_init(s);
}

void pc_input_shutdown(void)
{
    for (int port = 0; port < 4; port++)
        pc_input_live_rumble(port, 0);
    pc_input_live_shutdown();
}

int pc_input_paused(void)
{
    return pc_input_live_paused();
}

void pc_input_poll(PcPad pads[4])
{
    double now = pc_host_ns() / 1e9;
    memset(pads, 0, 4 * sizeof *pads);
    pc_input_live_poll(pads);
    for (int i = 0; i <= g_max_port; i++)
        pads[i].connected = 1;
    for (int i = 0; i < g_npress; i++) {
        const Press *p = &g_press[i];
        if (now < p->t || now >= p->t + p->dur)
            continue;
        pads[p->port].buttons |= p->mask;
        if (p->x)
            pads[p->port].stick_x = p->x;
        if (p->y)
            pads[p->port].stick_y = p->y;
    }
    rumble();
    if (pc_input_log)
        log_changes(pads);
}
