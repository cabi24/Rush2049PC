/* PS_RumbleStop: stop every Rumble Pak (MAT3 paksysdc.c). */
#include "game/game.h"

/* Makes the serial bus lock (a one-message queue) the first time it is needed (a copy of pfs_queue_init). */
static void pak_queue_init(void)
{
    if (!si_lock_inited) {
        si_lock_inited = 1; queue_init(&si_lock_queue, &si_lock_msg, 1);
        lock_give(&si_lock_queue);
    }
}

/* Takes the serial bus lock (a copy of si_lock). */
static void pak_lock(void)
{
    pak_queue_init();
    lock_take(&si_lock_queue);
}

/* Stops pak i's motor if it is a Rumble Pak: no rumble asked, the motor told to stop, and noted off if it did.
 * Returns -1 for a pak that isn't one, and nothing in particular otherwise. */
static s32 motor_init(tPak *p, s32 i)
{
    if (!p->rumble) return -1;
    p->request = 0;
    osMotorInit(&si_event_queue, &p->pfs, i);
    if (__osMotorAccess(&p->pfs, 0) == 0) {
        p->motorOn = 0;
    }
}

/* Stops all rumbling and keeps the rumble thread from starting it again until PS_RumbleGo. */
void PS_RumbleStop(void)
{
    s32 i;
    tPak *p;

    gRumble = 0;
    pak_lock();
    for (i = 0; i < NUM_PAKS; i++) {
        p = &gPaks[i];
        motor_init(p, i);
    }
    lock_give(&si_lock_queue);
}
