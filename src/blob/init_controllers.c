/* init_controllers.c: start the controllers (N64 only: libultra's serial interface; no MAT3 counterpart).
 *
 * Makes the serial interface's event queue, initialises the controllers through libultra (osContInit, under the
 * serial-bus lock the Controller Pak and Rumble Pak code share) and clears each controller's Rumble Pak state. */
#include "game/game.h"

/* Make the serial-bus lock the first time: a queue of one message, the token, posted at once (free). */
static void pak_queue_init(void)
{
    if (!si_lock_inited) {
        si_lock_inited = 1; queue_init(&si_lock_queue, &si_lock_msg, 1);
        lock_give(&si_lock_queue);
    }
}

/* Take the serial-bus lock (wait for its token); si_unlock gives it back. */
static void pak_lock(void)
{
    pak_queue_init();
    lock_take(&si_lock_queue);
}

/* Start the controllers: the serial interface's event queue (8 messages, the message for a finished transfer
 * si_message), osContInit under the lock (the connected controllers into cont_bit_pattern, their status into
 * cont_status), and the four controllers' Rumble Pak state cleared. */
void init_controllers(void)
{
    static s16 si_message;
    s32 i;

    queue_init(&si_event_queue, si_event_msgs, 8);
    si_message = 5;
    event_notify(EVENT_SI, &si_event_queue, &si_message);
    pak_lock();
    osContInit(&si_event_queue, &cont_bit_pattern, cont_status);
    si_unlock();
    for (i = 0; i < 4; i++) {
        gControllerData[i].attached = 0;
    }
}
