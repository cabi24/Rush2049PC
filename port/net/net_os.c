/* port/net/net_os.c: the clock, sleeps, random bytes and threads for port/net (net_os.h). */
#if defined(_WIN32) && (!defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601   /* Windows 7: CONDITION_VARIABLE, CreateWaitableTimerExW */
#endif
#ifdef _WIN32
#define _CRT_RAND_S   /* rand_s, msvcrt's RtlGenRandom */
#endif
#include "net_os.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32

uint64_t net_os_now_ns(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER c;
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    uint64_t f = (uint64_t)freq.QuadPart, v = (uint64_t)c.QuadPart;
    return v / f * 1000000000u + v % f * 1000000000u / f;
}

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x2
#endif

void net_os_sleep_ns(uint64_t ns)
{
    static __thread HANDLE timer;
    if (!ns)
        return;
    if (!timer)
        timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer)
        timer = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
    if (!timer) {
        Sleep((DWORD)((ns + 999999) / 1000000));
        return;
    }
    LARGE_INTEGER due;
    due.QuadPart = -(LONGLONG)((ns + 99) / 100);   /* relative, 100 ns units */
    SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
    WaitForSingleObject(timer, INFINITE);
}

void net_os_random(void *buf, size_t n)
{
    uint8_t *p = buf;
    for (size_t i = 0; i < n; i++) {
        unsigned int v = 0;
        if (rand_s(&v) != 0)
            v = (unsigned int)(net_os_now_ns() * 0x9E3779B97F4A7C15ull >> 32) ^ (unsigned int)GetCurrentProcessId();
        p[i] = (uint8_t)(v ^ (v >> 8) ^ (v >> 16) ^ (v >> 24));
    }
}

void net_mutex_init(NetMutex *m) { InitializeCriticalSection(m); }
void net_mutex_destroy(NetMutex *m) { DeleteCriticalSection(m); }
void net_mutex_lock(NetMutex *m) { EnterCriticalSection(m); }
void net_mutex_unlock(NetMutex *m) { LeaveCriticalSection(m); }
void net_cond_init(NetCond *c) { InitializeConditionVariable(c); }
void net_cond_destroy(NetCond *c) { (void)c; }
void net_cond_broadcast(NetCond *c) { WakeAllConditionVariable(c); }

void net_cond_wait_ns(NetCond *c, NetMutex *m, uint64_t ns)
{
    DWORD ms = (DWORD)((ns + 999999) / 1000000);
    SleepConditionVariableCS(c, m, ms ? ms : 1);
}

typedef struct {
    void *(*fn)(void *);
    void *arg;
} ThreadStart;

static DWORD WINAPI thread_main(LPVOID p)
{
    ThreadStart ts = *(ThreadStart *)p;
    free(p);
    ts.fn(ts.arg);
    return 0;
}

int net_thread_start(NetThread *t, void *(*fn)(void *), void *arg)
{
    ThreadStart *ts = malloc(sizeof *ts);
    if (!ts)
        return -1;
    ts->fn = fn;
    ts->arg = arg;
    *t = CreateThread(NULL, 0, thread_main, ts, 0, NULL);
    if (!*t) {
        free(ts);
        return -1;
    }
    return 0;
}

void net_thread_join(NetThread t)
{
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}

#else /* POSIX */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

uint64_t net_os_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

void net_os_sleep_ns(uint64_t ns)
{
    struct timespec ts = {(time_t)(ns / 1000000000u), (long)(ns % 1000000000u)};
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
    }
}

void net_os_random(void *buf, size_t n)
{
    uint8_t *p = buf;
    size_t got = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        while (got < n) {
            ssize_t r = read(fd, p + got, n - got);
            if (r <= 0) {
                if (r < 0 && errno == EINTR)
                    continue;
                break;
            }
            got += (size_t)r;
        }
        close(fd);
    }
    uint64_t x = net_os_now_ns() ^ ((uint64_t)getpid() << 32) ^ (uint64_t)(uintptr_t)buf;
    for (size_t i = got; i < n; i++) {   /* the fallback: splitmix64 on the clock */
        x += 0x9E3779B97F4A7C15ull;
        uint64_t z = x;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        p[i] = (uint8_t)(z ^ (z >> 31));
    }
}

void net_mutex_init(NetMutex *m) { pthread_mutex_init(m, NULL); }
void net_mutex_destroy(NetMutex *m) { pthread_mutex_destroy(m); }
void net_mutex_lock(NetMutex *m) { pthread_mutex_lock(m); }
void net_mutex_unlock(NetMutex *m) { pthread_mutex_unlock(m); }

void net_cond_init(NetCond *c)
{
    pthread_condattr_t a;
    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
    pthread_cond_init(c, &a);
    pthread_condattr_destroy(&a);
}

void net_cond_destroy(NetCond *c) { pthread_cond_destroy(c); }
void net_cond_broadcast(NetCond *c) { pthread_cond_broadcast(c); }

void net_cond_wait_ns(NetCond *c, NetMutex *m, uint64_t ns)
{
    uint64_t at = net_os_now_ns() + ns;
    struct timespec ts = {(time_t)(at / 1000000000u), (long)(at % 1000000000u)};
    pthread_cond_timedwait(c, m, &ts);
}

int net_thread_start(NetThread *t, void *(*fn)(void *), void *arg) { return pthread_create(t, NULL, fn, arg) ? -1 : 0; }
void net_thread_join(NetThread t) { pthread_join(t, NULL); }

#endif

void net_os_sleep_until(uint64_t t)
{
    for (uint64_t now = net_os_now_ns(); now < t; now = net_os_now_ns())
        net_os_sleep_ns(t - now);
}
