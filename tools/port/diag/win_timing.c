/* TEMPORARY diagnostic (to be removed): how long the scheduler's primitives take on a Windows runner. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <mmsystem.h>

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static uint64_t qpc_ns(void)
{
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return (uint64_t)((double)c.QuadPart * 1e9 / (double)f.QuadPart);
}

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv[2];
static int turn, stop;

static void *pong(void *p)
{
    (void)p;
    pthread_mutex_lock(&m);
    while (!stop) {
        while (turn != 1 && !stop)
            pthread_cond_wait(&cv[1], &m);
        turn = 0;
        pthread_cond_signal(&cv[0]);
    }
    pthread_mutex_unlock(&m);
    return NULL;
}

static HANDLE ev, ev2;
static volatile LONG64 sent_at;
static void *setter(void *p)
{
    (void)p;
    for (int i = 0; i < 50; i++) {
        WaitForSingleObject(ev2, INFINITE);
        Sleep(0);
        sent_at = (LONG64)now_ns();
        SetEvent(ev);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    pthread_t t;
    uint64_t t0, t1, mx;
    int n;
    struct timespec res;
    if (argc > 1) {
        uint64_t a = now_ns();
        HMODULE gl = LoadLibraryA("opengl32.dll");
        char path[MAX_PATH] = "";
        if (gl)
            GetModuleFileNameA(gl, path, sizeof path);
        printf("LoadLibrary(opengl32.dll): %s in %.1f ms\n", gl ? path : "failed", (now_ns() - a) / 1e6);
    }
    clock_getres(CLOCK_MONOTONIC, &res);
    printf("clock_getres(MONOTONIC) %ld ns\n", (long)res.tv_nsec);
    t0 = now_ns();
    n = 0;
    while ((t1 = now_ns()) == t0)
        n++;
    printf("clock_gettime step %.3f us (%d same reads)\n", (t1 - t0) / 1e3, n);
    t0 = qpc_ns();
    while ((t1 = qpc_ns()) == t0)
        ;
    printf("QPC step %.3f us\n", (t1 - t0) / 1e3);

    for (int pass = 0; pass < 2; pass++) {
        if (pass)
            timeBeginPeriod(1);
        printf("== %s\n", pass ? "after timeBeginPeriod(1)" : "default timer period");
        HANDLE e = CreateEventW(NULL, FALSE, FALSE, NULL);
        mx = 0;
        t0 = now_ns();
        for (int i = 0; i < 20; i++) {
            uint64_t a = now_ns();
            WaitForSingleObject(e, 1);
            uint64_t d = now_ns() - a;
            if (d > mx)
                mx = d;
        }
        printf("WaitForSingleObject(event, 1 ms) x20: %.2f ms each, max %.2f\n", (now_ns() - t0) / 20e6, mx / 1e6);
        t0 = now_ns();
        for (int i = 0; i < 20; i++)
            Sleep(1);
        printf("Sleep(1) x20: %.2f ms each\n", (now_ns() - t0) / 20e6);
        HANDLE ht = CreateWaitableTimerExW(NULL, NULL, 2, TIMER_ALL_ACCESS);
        printf("high-resolution timer: %s\n", ht ? "yes" : "no");
        if (!ht)
            ht = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
        t0 = now_ns();
        mx = 0;
        for (int i = 0; i < 20; i++) {
            LARGE_INTEGER due;
            due.QuadPart = -10000;
            uint64_t a = now_ns();
            SetWaitableTimer(ht, &due, 0, NULL, NULL, FALSE);
            WaitForSingleObject(ht, INFINITE);
            uint64_t d = now_ns() - a;
            if (d > mx)
                mx = d;
        }
        printf("waitable timer 1 ms x20: %.2f ms each, max %.2f\n", (now_ns() - t0) / 20e6, mx / 1e6);
        t0 = now_ns();
        for (int i = 0; i < 20; i++) {
            struct timespec ts = {0, 1000000};
            nanosleep(&ts, NULL);
        }
        printf("nanosleep 1 ms x20: %.2f ms each\n", (now_ns() - t0) / 20e6);
        pthread_cond_t c;
        pthread_cond_init(&c, NULL);
        pthread_mutex_lock(&m);
        t0 = now_ns();
        for (int i = 0; i < 20; i++) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            pthread_cond_timedwait(&c, &m, &ts);
        }
        pthread_mutex_unlock(&m);
        printf("pthread_cond_timedwait 1 ms x20: %.2f ms each\n", (now_ns() - t0) / 20e6);
        t0 = now_ns();
        for (int i = 0; i < 1000; i++)
            sched_yield();
        printf("sched_yield x1000: %.2f us each\n", (now_ns() - t0) / 1000e3);
    }

    pthread_cond_init(&cv[0], NULL);
    pthread_cond_init(&cv[1], NULL);
    pthread_create(&t, NULL, pong, NULL);
    pthread_mutex_lock(&m);
    t0 = now_ns();
    mx = 0;
    for (int i = 0; i < 2000; i++) {
        uint64_t a = now_ns();
        turn = 1;
        pthread_cond_signal(&cv[1]);
        while (turn != 0)
            pthread_cond_wait(&cv[0], &m);
        uint64_t d = now_ns() - a;
        if (d > mx)
            mx = d;
    }
    printf("cond ping-pong x2000: %.2f us per round trip, max %.2f ms\n", (now_ns() - t0) / 2000e3, mx / 1e6);
    stop = 1;
    pthread_cond_signal(&cv[1]);
    pthread_mutex_unlock(&m);
    pthread_join(t, NULL);

    ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    ev2 = CreateEventW(NULL, FALSE, FALSE, NULL);
    pthread_create(&t, NULL, setter, NULL);
    mx = 0;
    uint64_t sum = 0;
    for (int i = 0; i < 50; i++) {
        SetEvent(ev2);
        WaitForSingleObject(ev, 1000);
        uint64_t d = now_ns() - (uint64_t)sent_at;
        sum += d;
        if (d > mx)
            mx = d;
    }
    pthread_join(t, NULL);
    printf("SetEvent -> wake x50: %.2f us mean, max %.2f ms\n", sum / 50e3, mx / 1e6);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    printf("processors: %lu\n", (unsigned long)si.dwNumberOfProcessors);
    return 0;
}
