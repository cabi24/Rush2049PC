/* port/gfx/tests/display_test.c: the display's exact refresh (gfx_display.c) from made-up modes, ratios and counter
 * values, as XRandR, Windows' display configuration and its compositor give them (no window, no GL).
 *
 *   gfx_display_test [mode|ratio|plausible|counter|pick|all]
 *
 * Exit status 0 = pass. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gfx_internal.h"

static int s_fail;

static void expect_i(const char *what, long long got, long long want) {
    if (got != want) {
        printf("  FAIL %s: %lld, not %lld\n", what, got, want);
        s_fail++;
    }
}

static void expect_near(const char *what, double got, double want, double tol) {
    if (!(fabs(got - want) <= tol)) {
        printf("  FAIL %s: %.9f, not %.9f (within %g)\n", what, got, want, tol);
        s_fail++;
    }
}

/* XRandR's modes: dot clock / (htotal x vtotal), interlaced fields and doubled lines. */
static void t_mode(void) {
    /* CEA 1080p60 (148.5 MHz) and its 59.94 twin (148.5 / 1.001), 2200 x 1125 */
    expect_near("1080p60", pcgfx_mode_hz(148500000, 2200, 1125, 0, 0), 60.0, 1e-9);
    expect_near("1080p59.94", pcgfx_mode_hz(148351648, 2200, 1125, 0, 0), 60000.0 / 1001.0, 1e-5);
    /* a 2560 x 1440 panel's "144 Hz" mode that is really 143.96 (586.586 MHz, 2720 x 1498) */
    expect_near("1440p143.96", pcgfx_mode_hz(586586000, 2720, 1498, 0, 0), 143.96, 0.01);
    /* 1080i: 1125 lines in two fields, a field each vblank */
    expect_near("1080i", pcgfx_mode_hz(74250000, 2200, 1125, 1, 0), 60.0, 1e-9);
    /* 640 x 480 doublescan of 320 x 240: each line twice */
    expect_near("doublescan", pcgfx_mode_hz(12587500, 800, 262, 0, 1), 12587500.0 / (800.0 * 524.0), 1e-9);
    expect_near("no timings", pcgfx_mode_hz(0, 2200, 1125, 0, 0), 0.0, 0);
    expect_near("no htotal", pcgfx_mode_hz(148500000, 0, 1125, 0, 0), 0.0, 0);
}

/* Windows' DISPLAYCONFIG_RATIONAL and the compositor's UNSIGNED_RATIO. */
static void t_ratio(void) {
    expect_near("60000/1001", pcgfx_ratio_hz(60000, 1001), 59.94005994, 1e-7);
    expect_near("143981/1000", pcgfx_ratio_hz(143981, 1000), 143.981, 1e-9);
    expect_near("60/1", pcgfx_ratio_hz(60, 1), 60.0, 0);
    expect_near("x/0", pcgfx_ratio_hz(60, 0), 0.0, 0);
}

/* An exact refresh is taken only when SDL's whole hertz could be its rounding. */
static void t_plausible(void) {
    expect_i("59.94 for SDL's 59", pcgfx_refresh_plausible(59.94, 59), 1);
    expect_i("59.94 for SDL's 60", pcgfx_refresh_plausible(59.94, 60), 1);
    expect_i("143.98 for 144", pcgfx_refresh_plausible(143.98, 144), 1);
    expect_i("60 for 144 (another monitor's)", pcgfx_refresh_plausible(60.0, 144), 0);
    expect_i("30 for 60", pcgfx_refresh_plausible(30.0, 60), 0);
    expect_i("SDL unknown", pcgfx_refresh_plausible(74.97, 0), 1);
    expect_i("0", pcgfx_refresh_plausible(0.0, 60), 0);
    expect_i("NaN", pcgfx_refresh_plausible(NAN, 60), 0);
    expect_i("2000", pcgfx_refresh_plausible(2000.0, 0), 0);
}

/* The compositor's vblank (a performance-counter value) as an age in ns. */
static void t_counter(void) {
    /* a 10 MHz counter: 1000 counts ago is 100 us */
    expect_i("past", pcgfx_counter_age_ns(5000000000ull, 5000001000ull, 10000000), 100000);
    expect_i("to come", pcgfx_counter_age_ns(5000001000ull, 5000000000ull, 10000000), -100000);
    expect_i("now", pcgfx_counter_age_ns(77, 77, 10000000), 0);
    /* a 3.579545 MHz counter (older hardware): 59659 counts is 16.6665 ms, rounded to the ns */
    expect_i("acpi", pcgfx_counter_age_ns(0, 59659, 3579545), (long long)llround(59659.0 * 1e9 / 3579545.0));
    /* a large counter value (weeks of uptime at 10 MHz) keeps the difference exact */
    expect_i("large", pcgfx_counter_age_ns(0x7FFFFFFF00000000ull, 0x7FFFFFFF00000000ull + 166833, 10000000),
             16683300);
    expect_i("no frequency", pcgfx_counter_age_ns(0, 100, 0), 0);
}

/* What is paced by, from the monitor's, the compositor's and SDL's refreshes. */
static void t_pick(void) {
    double p;
    int64_t age;
    int k;

    /* Windows, one 59.94 Hz monitor (SDL says 59): the monitor's period and the compositor's vblank */
    p = 0, age = 0;
    k = pcgfx_display_pick(60000.0 / 1001, 60000.0 / 1001, 1, 4200000, 59, &p, &age);
    expect_i("one monitor: bits", k, PCGFX_TIMING_PERIOD | PCGFX_TIMING_VBLANK);
    expect_near("one monitor: period", p, 1e9 * 1001 / 60000, 1e-3);
    expect_i("one monitor: age", age, 4200000);

    /* a 143.98 Hz monitor that SDL calls 144, the compositor at 143.981 (its own measure): within 0.05% */
    p = 0, age = 0;
    k = pcgfx_display_pick(143.98, 143.981, 1, -300000, 144, &p, &age);
    expect_i("143.98: bits", k, PCGFX_TIMING_PERIOD | PCGFX_TIMING_VBLANK);
    expect_near("143.98: period", p, 1e9 / 143.98, 1e-3);
    expect_i("143.98: a vblank to come", age, -300000);

    /* the window on a 59.94 monitor, the compositor following a 60.000 one: the period, not the other's vblanks */
    p = 0, age = 0;
    k = pcgfx_display_pick(60000.0 / 1001, 60.0, 1, 1000000, 59, &p, &age);
    expect_i("two monitors: bits", k, PCGFX_TIMING_PERIOD);
    expect_near("two monitors: period", p, 1e9 * 1001 / 60000, 1e-3);
    expect_i("two monitors: age untouched", age, 0);

    /* the window on a 144 Hz monitor, the compositor at 60: the monitor's period only */
    p = 0, age = 0;
    k = pcgfx_display_pick(144.0, 60.0, 1, 1000000, 144, &p, &age);
    expect_i("144 and 60: bits", k, PCGFX_TIMING_PERIOD);
    expect_near("144 and 60: period", p, 1e9 / 144, 1e-3);

    /* no display configuration (QueryDisplayConfig failed): the compositor's, when SDL's agrees */
    p = 0, age = 0;
    k = pcgfx_display_pick(0.0, 59.94, 1, 2000000, 60, &p, &age);
    expect_i("compositor only: bits", k, PCGFX_TIMING_PERIOD | PCGFX_TIMING_VBLANK);
    expect_near("compositor only: period", p, 1e9 / 59.94, 1e-3);
    k = pcgfx_display_pick(0.0, 60.0, 1, 2000000, 144, &p, &age);
    expect_i("compositor of another monitor", k, 0);

    /* a stale vblank (over a second away): the period only */
    p = 0, age = 0;
    k = pcgfx_display_pick(59.94, 59.94, 1, 3000000000ll, 60, &p, &age);
    expect_i("stale vblank: bits", k, PCGFX_TIMING_PERIOD);
    expect_i("stale vblank: age untouched", age, 0);

    /* the compositor gave no vblank (its counter 0) */
    k = pcgfx_display_pick(59.94, 59.94, 0, 0, 59, &p, &age);
    expect_i("no vblank", k, PCGFX_TIMING_PERIOD);

    /* X11: XRandR's rate, no compositor */
    p = 0, age = 0;
    k = pcgfx_display_pick(pcgfx_mode_hz(148351648, 2200, 1125, 0, 0), 0.0, 0, 0, 60, &p, &age);
    expect_i("xrandr: bits", k, PCGFX_TIMING_PERIOD);
    expect_near("xrandr: period", p, 2200.0 * 1125.0 * 1e9 / 148351648.0, 1e-3);

    /* a monitor's mode that SDL's whole hertz can't be the rounding of (a mode list out of date): nothing */
    k = pcgfx_display_pick(75.0, 0.0, 0, 0, 60, &p, &age);
    expect_i("implausible", k, 0);
    k = pcgfx_display_pick(0.0, 0.0, 0, 0, 60, &p, &age);
    expect_i("nothing", k, 0);
}

int main(int argc, char **argv) {
    static const struct {
        const char *name;
        void (*fn)(void);
    } cases[] = {{"mode", t_mode}, {"ratio", t_ratio}, {"plausible", t_plausible}, {"counter", t_counter},
                 {"pick", t_pick}};
    const char *want = argc > 1 ? argv[1] : "all";
    size_t i;
    int ran = 0;
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++)
        if (strcmp(want, "all") == 0 || strcmp(want, cases[i].name) == 0) {
            int before = s_fail;
            cases[i].fn();
            printf("%s %s\n", s_fail == before ? "ok  " : "FAIL", cases[i].name);
            ran++;
        }
    if (!ran) {
        fprintf(stderr, "gfx_display_test: no case %s\n", want);
        return 2;
    }
    return s_fail ? 1 : 0;
}
