/* port/net/net_os.h: what the network library needs from the OS beyond sockets: a monotonic clock in
 * nanoseconds, precise sleeps, random bytes, a mutex, a condition variable with a relative timeout, a thread.
 * POSIX threads on Linux; Windows' own primitives on Windows (winpthreads' timed waits move in 15.6 ms steps,
 * docs/port/WINDOWS.md 5). */
#ifndef NET_OS_H
#define NET_OS_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>   /* before windows.h */
#include <windows.h>
typedef CRITICAL_SECTION NetMutex;
typedef CONDITION_VARIABLE NetCond;
typedef HANDLE NetThread;
#else
#include <pthread.h>
typedef pthread_mutex_t NetMutex;
typedef pthread_cond_t NetCond;
typedef pthread_t NetThread;
#endif

/* CLOCK_MONOTONIC (QueryPerformanceCounter on Windows), in ns. */
uint64_t net_os_now_ns(void);
/* Sleep until net_os_now_ns() reaches t (a high-resolution waitable timer on Windows). */
void net_os_sleep_until(uint64_t t);
void net_os_sleep_ns(uint64_t ns);
/* n bytes from the OS's random source (/dev/urandom, rand_s), mixed with the clock if that fails. */
void net_os_random(void *buf, size_t n);

void net_mutex_init(NetMutex *m);
void net_mutex_destroy(NetMutex *m);
void net_mutex_lock(NetMutex *m);
void net_mutex_unlock(NetMutex *m);
void net_cond_init(NetCond *c);
void net_cond_destroy(NetCond *c);
void net_cond_broadcast(NetCond *c);
/* Wait up to ns with m held on entry and on return; may return early (spurious, or a broadcast). */
void net_cond_wait_ns(NetCond *c, NetMutex *m, uint64_t ns);
int net_thread_start(NetThread *t, void *(*fn)(void *), void *arg);
void net_thread_join(NetThread t);

#endif
