/* include/game/threads.h: the game's threads and the messages that start and end a frame. The boot segment's start-up
 * (src/rom/boot.c) makes the threads and serves the messages; the game image (game/system.h) reads them. */
#ifndef GAME_THREADS_H
#define GAME_THREADS_H

/* The game's threads (src/rom/boot.c): libultra thread ids, and priorities (the higher runs first). On the PC each is
 * a host thread and port/ultra keeps their priority order (docs/port/ULTRA.md). */
#define THREAD_ID_IDLE 1        /* main's first thread: starts the PI manager and the main thread, then idles */
#define THREAD_ID_MODEL 5       /* the car simulation (ModelThread) */
#define THREAD_ID_MAIN 6        /* start-up, then the scheduler's client that starts and ends frames */
#define THREAD_ID_GAME 7        /* GameLoop forever */
#define THREAD_ID_BG_LOAD 8     /* the background wave loader (BGLoadWaveMain) */
#define THREAD_PRI_IDLE_START 2 /* the idle thread until it has started the main thread; then 0 */
#define THREAD_PRI_BG_LOAD 3
#define THREAD_PRI_MAIN 4
#define THREAD_PRI_GAME 5
#define THREAD_PRI_MODEL 7
#define THREAD_PRI_SCHEDULER 12
#define THREAD_PRI_PI_MANAGER 150

/* The messages on the main thread's queue (main_thread_queue): the scheduler's (libultra's sched.h names), and the
 * game thread's request to end a frame (UpdateFrame); the main thread sends frame_start_msg to the game thread. */
#define OS_SC_RETRACE_MSG 1
#define OS_SC_DONE_MSG 2
#define OS_SC_PRE_NMI_MSG 4
#define GAME_MSG_END_FRAME 0xABE    /* 2750 */
#define GAME_MSG_START_FRAME 0xABF

#endif /* GAME_THREADS_H */
