/* port/ultra/vi.c: the video interface: the frame shown, the screen blacked out, the retrace event
 * (docs/port/ULTRA.md 3 and 11).
 *
 * No VI manager thread: the host main loop calls pc_vi_retrace at 60 Hz (NTSC). Each retrace sends the
 * osViSetEvent message every retraceCount retraces, as libultra's VI manager does, and a framebuffer passed to
 * pc_vi_swap_buffer becomes current there; the main loop then shows it (pc_gfx_present) unless the screen is blacked
 * out. The N64's video interface also had a mode (its timing and scaling registers) and special features (gamma, the
 * dither filter, the divot): the PC's picture is the renderer's, so the game sets none of them. */
#define _GNU_SOURCE
#include <pthread.h>

#include "ultra.h"

static pthread_mutex_t g_vlock = PTHREAD_MUTEX_INITIALIZER;
static struct {
    int black;
    void *cur, *next;
    void *mq;
    OSMesg msg;
    uint32_t count, left;
} g_vi;

/* The screen blacked out (black: 1) or showing the frames swapped in (0). */
void pc_vi_black(int black)
{
    pthread_mutex_lock(&g_vlock);
    g_vi.black = black != 0;
    pthread_mutex_unlock(&g_vlock);
}

void osViSetEvent(void *mq, OSMesg msg, uint32_t retrace_count)
{
    pthread_mutex_lock(&g_vlock);
    g_vi.mq = mq;
    g_vi.msg = msg;
    g_vi.count = g_vi.left = retrace_count ? retrace_count : 1;
    pthread_mutex_unlock(&g_vlock);
}

/* The frame rendered into framebuffer is shown from the next retrace on. */
void pc_vi_swap_buffer(void *framebuffer)
{
    pthread_mutex_lock(&g_vlock);
    g_vi.next = framebuffer;
    pthread_mutex_unlock(&g_vlock);
}

/* The framebuffer shown now. */
void *pc_vi_current_framebuffer(void)
{
    pthread_mutex_lock(&g_vlock);
    void *fb = g_vi.cur;
    pthread_mutex_unlock(&g_vlock);
    return fb;
}

/* The framebuffer shown from the next retrace on: the current one when no swap waits. */
void *pc_vi_next_framebuffer(void)
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
