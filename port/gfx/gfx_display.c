/* port/gfx/gfx_display.c: the display's exact refresh and its vblanks, for a high frame rate paced by the display
 * (docs/port/FRAMERATE.md 2, GFX.md "High frame rate"). SDL gives a refresh in whole hertz (59 or 60 for a 59.94 Hz
 * display); a grid of vblanks at that rate drifts from the display's by a refresh about once a second, which a swap
 * that never blocks (a driver that queues frames) never corrects. The platforms know better:
 *
 *   - Windows: the window's monitor's mode from QueryDisplayConfig (user32: the target's vSyncFreq, a ratio such as
 *     60000/1001), and the compositor's last vblank from DwmGetCompositionTimingInfo (dwmapi.dll, loaded at run time,
 *     so the program imports no new DLL), used when the compositor's refresh is the window's monitor's;
 *   - X11: the CRTC under the window, its mode's dot clock / (htotal x vtotal) (XRandR, libXrandr loaded at run
 *     time, as SDL loads it), no vblank times;
 *   - elsewhere (Wayland, headless): nothing; the loop keeps SDL's whole hertz, corrected by the swaps that block.
 *
 * The arithmetic is apart from the queries (pcgfx_mode_hz ... pcgfx_display_pick), so the tests run it on made-up
 * values (tests/display_test.c). */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"

/* ---------------------------------------------------------------- the arithmetic */

/* A video mode's refresh: pixels a second over pixels a frame. An interlaced mode shows a field (half the lines) at
 * each vblank, a doublescan one each line twice. 0 for a mode without timings. */
double pcgfx_mode_hz(uint64_t dot_clock, uint32_t htotal, uint32_t vtotal, int interlace, int doublescan) {
    double lines = (double)vtotal;
    if (dot_clock == 0 || htotal == 0 || vtotal == 0)
        return 0.0;
    if (doublescan)
        lines *= 2.0;
    if (interlace)
        lines /= 2.0;
    return (double)dot_clock / ((double)htotal * lines);
}

double pcgfx_ratio_hz(uint32_t num, uint32_t den) {
    return den != 0 ? (double)num / (double)den : 0.0;
}

/* An exact refresh taken only when it is one SDL's whole hertz could be the rounding of (within a hertz, as 59.94 is
 * of 59 and of 60, 143.98 of 144), and a refresh at all: something else (another monitor's) otherwise. SDL's 0
 * (unknown): any refresh from 20 to 1000 Hz. */
int pcgfx_refresh_plausible(double exact_hz, int whole_hz) {
    if (!(exact_hz >= 20.0 && exact_hz <= 1000.0))
        return 0;
    return whole_hz <= 0 || fabs(exact_hz - (double)whole_hz) <= 1.0;
}

/* How long ago an event at performance-counter value at was, at counter value now (the counter's freq a second), in
 * ns: negative for one still to come. */
int64_t pcgfx_counter_age_ns(uint64_t at, uint64_t now, uint64_t freq) {
    double d;
    if (freq == 0)
        return 0;
    d = now >= at ? (double)(now - at) : -(double)(at - now);
    return (int64_t)llround(d * 1e9 / (double)freq);
}

/* What to pace by, from what the platform gave (0 for what it didn't): the window's monitor's refresh (mon_hz), the
 * compositor's (comp_hz) with its last vblank comp_age_ns ago, and SDL's whole hertz. The monitor's refresh when it is
 * plausible, else the compositor's; the compositor's vblank only when its refresh is the one paced by (within 0.05%:
 * the compositor of a desktop whose monitors differ follows one of them). Returns PCGFX_TIMING_* bits, *period_ns
 * and *vblank_age_ns set for those. */
int pcgfx_display_pick(double mon_hz, double comp_hz, int comp_vblank, int64_t comp_age_ns, int whole_hz,
                       double *period_ns, int64_t *vblank_age_ns) {
    double hz = 0.0;
    int bits = 0;
    if (pcgfx_refresh_plausible(mon_hz, whole_hz))
        hz = mon_hz;
    else if (pcgfx_refresh_plausible(comp_hz, whole_hz))
        hz = comp_hz;
    if (hz <= 0.0)
        return 0;
    *period_ns = 1e9 / hz;
    bits = PCGFX_TIMING_PERIOD;
    if (comp_vblank && comp_hz > 0.0 && fabs(comp_hz - hz) <= hz * 0.0005 &&
        llabs(comp_age_ns) < (int64_t)1000000000) {   /* a vblank within the last second */
        *vblank_age_ns = comp_age_ns;
        bits |= PCGFX_TIMING_VBLANK;
    }
    return bits;
}

/* ---------------------------------------------------------------- Windows */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>

typedef HRESULT(WINAPI *DwmTimingProc)(HWND, DWM_TIMING_INFO *);

/* The monitor's refresh from its display path: the target mode's vSyncFreq (the signal's own), else the path's
 * refresh. Taken again when the monitor or SDL's refresh changes, and every 2 s (a mode change SDL rounds alike). */
static double qdc_hz(HMONITOR mon) {
    MONITORINFOEXW mi;
    UINT32 np = 0, nm = 0, i;
    DISPLAYCONFIG_PATH_INFO *paths;
    DISPLAYCONFIG_MODE_INFO *modes;
    double hz = 0.0;
    memset(&mi, 0, sizeof mi);
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(mon, (MONITORINFO *)&mi))
        return 0.0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &np, &nm) != ERROR_SUCCESS || np == 0)
        return 0.0;
    paths = calloc(np, sizeof *paths);
    modes = calloc(nm ? nm : 1, sizeof *modes);
    if (paths != NULL && modes != NULL &&
        QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &np, paths, &nm, modes, NULL) == ERROR_SUCCESS) {
        for (i = 0; i < np && hz <= 0.0; i++) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME src;
            UINT32 m = paths[i].targetInfo.modeInfoIdx;
            memset(&src, 0, sizeof src);
            src.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            src.header.size = sizeof src;
            src.header.adapterId = paths[i].sourceInfo.adapterId;
            src.header.id = paths[i].sourceInfo.id;
            if (DisplayConfigGetDeviceInfo(&src.header) != ERROR_SUCCESS || wcscmp(src.viewGdiDeviceName, mi.szDevice))
                continue;
            if (m != DISPLAYCONFIG_PATH_MODE_IDX_INVALID && m < nm &&
                modes[m].infoType == DISPLAYCONFIG_MODE_INFO_TYPE_TARGET) {
                const DISPLAYCONFIG_VIDEO_SIGNAL_INFO *s = &modes[m].targetMode.targetVideoSignalInfo;
                hz = pcgfx_ratio_hz(s->vSyncFreq.Numerator, s->vSyncFreq.Denominator);
            }
            if (hz <= 0.0)
                hz = pcgfx_ratio_hz(paths[i].targetInfo.refreshRate.Numerator,
                                    paths[i].targetInfo.refreshRate.Denominator);
        }
    }
    free(paths);
    free(modes);
    return hz;
}

int pcgfx_win_display_timing(void *hwnd, int whole_hz, double *period_ns, int64_t *vblank_age_ns) {
    static HMONITOR s_mon;
    static int s_whole, s_tried;
    static double s_mon_hz;
    static DWORD s_when;
    static DwmTimingProc s_dwm;
    HMONITOR mon = MonitorFromWindow((HWND)hwnd, MONITOR_DEFAULTTONEAREST);
    double comp_hz = 0.0;
    int comp_vblank = 0;
    int64_t age = 0;
    DWORD now = GetTickCount();
    if (mon != s_mon || whole_hz != s_whole || now - s_when > 2000) {
        s_mon = mon;
        s_whole = whole_hz;
        s_when = now;
        s_mon_hz = mon != NULL ? qdc_hz(mon) : 0.0;
    }
    if (!s_tried) {   /* every Windows since Vista has it; loaded here so the program doesn't import it */
        HMODULE dll = LoadLibraryW(L"dwmapi.dll");
        s_tried = 1;
        if (dll != NULL)
            s_dwm = (DwmTimingProc)(void (*)(void))GetProcAddress(dll, "DwmGetCompositionTimingInfo");
    }
    if (s_dwm != NULL) {
        DWM_TIMING_INFO ti;
        LARGE_INTEGER c, f;
        memset(&ti, 0, sizeof ti);
        ti.cbSize = sizeof ti;
        /* NULL: since Windows 8.1 the compositor's timing, not a window's (a window handle is refused) */
        if (SUCCEEDED(s_dwm(NULL, &ti)) && QueryPerformanceCounter(&c) && QueryPerformanceFrequency(&f)) {
            comp_hz = pcgfx_ratio_hz(ti.rateRefresh.uiNumerator, ti.rateRefresh.uiDenominator);
            if (comp_hz <= 0.0 && ti.qpcRefreshPeriod > 0)
                comp_hz = (double)f.QuadPart / (double)ti.qpcRefreshPeriod;
            if (ti.qpcVBlank != 0) {
                comp_vblank = 1;
                age = pcgfx_counter_age_ns(ti.qpcVBlank, (uint64_t)c.QuadPart, (uint64_t)f.QuadPart);
            }
        }
    }
    return pcgfx_display_pick(s_mon_hz, comp_hz, comp_vblank, age, whole_hz, period_ns, vblank_age_ns);
}
#endif

/* ---------------------------------------------------------------- X11 (XRandR) */
#ifdef PCGFX_HAVE_XRANDR
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

static struct {
    int tried;
    XRRScreenResources *(*get_res)(Display *, Window);
    XRRCrtcInfo *(*get_crtc)(Display *, XRRScreenResources *, RRCrtc);
    void (*free_res)(XRRScreenResources *);
    void (*free_crtc)(XRRCrtcInfo *);
} X;

/* The refresh of the CRTC that shows the point (x, y) of the root window (the window's centre), from its mode's
 * timings. */
static double xrandr_hz(Display *dpy, int x, int y) {
    XRRScreenResources *res;
    double hz = 0.0;
    int i, j;
    if (!X.tried) {
        void *lib = dlopen("libXrandr.so.2", RTLD_NOW | RTLD_LOCAL);
        X.tried = 1;
        if (lib != NULL) {
            *(void **)&X.get_res = dlsym(lib, "XRRGetScreenResourcesCurrent");
            *(void **)&X.get_crtc = dlsym(lib, "XRRGetCrtcInfo");
            *(void **)&X.free_res = dlsym(lib, "XRRFreeScreenResources");
            *(void **)&X.free_crtc = dlsym(lib, "XRRFreeCrtcInfo");
        }
    }
    if (X.get_res == NULL || X.get_crtc == NULL || X.free_res == NULL || X.free_crtc == NULL || dpy == NULL)
        return 0.0;
    res = X.get_res(dpy, DefaultRootWindow(dpy));
    if (res == NULL)
        return 0.0;
    for (i = 0; i < res->ncrtc && hz <= 0.0; i++) {
        XRRCrtcInfo *c = X.get_crtc(dpy, res, res->crtcs[i]);
        if (c == NULL)
            continue;
        if (c->mode != None && x >= c->x && y >= c->y && x < c->x + (int)c->width && y < c->y + (int)c->height)
            for (j = 0; j < res->nmode; j++)
                if (res->modes[j].id == c->mode) {
                    const XRRModeInfo *m = &res->modes[j];
                    hz = pcgfx_mode_hz(m->dotClock, m->hTotal, m->vTotal, (m->modeFlags & RR_Interlace) != 0,
                                       (m->modeFlags & RR_DoubleScan) != 0);
                    break;
                }
        X.free_crtc(c);
    }
    X.free_res(res);
    return hz;
}

/* Called by the main loop every 100 ms or so: the mode is read again every 20 calls. */
int pcgfx_x11_display_timing(void *display, int x, int y, int whole_hz, double *period_ns) {
    static int s_whole, s_x, s_y;
    static unsigned s_calls;
    static double s_hz;
    int64_t dummy = 0;
    if (whole_hz != s_whole || x != s_x || y != s_y || s_calls++ % 20 == 0) {
        s_whole = whole_hz, s_x = x, s_y = y;
        s_hz = xrandr_hz((Display *)display, x, y);
    }
    return pcgfx_display_pick(s_hz, 0.0, 0, 0, whole_hz, period_ns, &dummy);
}
#endif
