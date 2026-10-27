/* port/ultra/vi.c: the video interface (docs/port/ULTRA.md).
 *
 * No VI manager thread: the host main loop calls pc_vi_retrace at 60 Hz (NTSC). Each retrace sends the
 * osViSetEvent message every retraceCount retraces, as libultra's VI manager does, and a framebuffer passed to
 * osViSwapBuffer becomes current there; the main loop then shows it (pc_gfx_present). The mode, special
 * features, blackout and scale are recorded for the renderer and for status lines. */
#define _GNU_SOURCE
#include <pthread.h>

#include "ultra.h"

static pthread_mutex_t g_vlock = PTHREAD_MUTEX_INITIALIZER;
static struct {
    const void *mode;
    uint32_t features;
    int black;
    float xscale, yscale;
    void *cur, *next;
    void *mq;
    OSMesg msg;
    uint32_t count, left;
} g_vi = {.xscale = 1.0f, .yscale = 1.0f};

void osCreateViManager(int32_t pri) { (void)pri; }

void osViSetMode(void *mode)
{
    pthread_mutex_lock(&g_vlock);
    g_vi.mode = mode;
    pthread_mutex_unlock(&g_vlock);
}

void osViSetSpecialFeatures(uint32_t features)
{
    pthread_mutex_lock(&g_vlock);
    g_vi.features = features;
    pthread_mutex_unlock(&g_vlock);
}

void osViBlack(uint8_t active)
{
    pthread_mutex_lock(&g_vlock);
    g_vi.black = active != 0;
    pthread_mutex_unlock(&g_vlock);
}

void osViSetXScale(float scale) { g_vi.xscale = scale; }
void osViSetYScale(float scale) { g_vi.yscale = scale; }

void osViSetEvent(void *mq, OSMesg msg, uint32_t retrace_count)
{
    pthread_mutex_lock(&g_vlock);
    g_vi.mq = mq;
    g_vi.msg = msg;
    g_vi.count = g_vi.left = retrace_count ? retrace_count : 1;
    pthread_mutex_unlock(&g_vlock);
}

void osViSwapBuffer(void *framebuffer)
{
    pthread_mutex_lock(&g_vlock);
    g_vi.next = framebuffer;
    pthread_mutex_unlock(&g_vlock);
}

void *osViGetCurrentFramebuffer(void)
{
    pthread_mutex_lock(&g_vlock);
    void *fb = g_vi.cur;
    pthread_mutex_unlock(&g_vlock);
    return fb;
}

void *osViGetNextFramebuffer(void)
{
    pthread_mutex_lock(&g_vlock);
    void *fb = g_vi.next;
    pthread_mutex_unlock(&g_vlock);
    return fb;
}

void pc_vi_retrace(const void **present_fb)
{
    void *mq = NULL;
    OSMesg msg = NULL;
    pthread_mutex_lock(&g_vlock);
    *present_fb = NULL;
    if (g_vi.next != g_vi.cur) {
        g_vi.cur = g_vi.next;
        if (!g_vi.black)
            *present_fb = g_vi.cur;
    }
    if (g_vi.mq && --g_vi.left == 0) {
        g_vi.left = g_vi.count;
        mq = g_vi.mq;
        msg = g_vi.msg;
    }
    pthread_mutex_unlock(&g_vlock);
    if (mq)
        pc_os_post(mq, msg);
}
