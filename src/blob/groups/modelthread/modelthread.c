/* modelthread.c: the model thread, which runs the car simulation (MAT3 model.c, ModelThread()).
 *
 * The N64 game simulates the cars on a thread of their own, woken by the scheduler at every retrace (60 times a
 * second): each time it reads the controllers and, while the race lets the cars move (modelrun), steps the
 * simulation (model_iteration). On the PC it is a host thread like the others (port/ultra). MAT3 calls the same
 * work once a frame from its main loop. */
#include "game/game.h"

extern void __setfpcsr(u32 value);

/* The thread's body; `arg` is unused. It sets the FPU's control word (0x01000E00: flush denormals to zero, trap on
 * invalid operations, division by zero and overflow; on the PC port/ultra only records it), makes its message
 * queue, sets up the controllers, stops the simulation and rewinds the model clock (init_model_task), and joins
 * the scheduler's clients. Then for ever: it waits for a message, takes every other one waiting (several
 * retraces may have passed; the N64 code tests each for a retrace or the pre-reset message and does nothing with
 * either), reads the controllers and, if they were read and the simulation runs, steps it. */
void ModelThread(void *arg)
{
    OSScClient client;
    OSMesg msg;

    msg = 0;
    __setfpcsr(0x01000E00);
    queue_init(&model_thread_queue, model_thread_msgs, 8);
    init_controllers();
    modelrun = 0;
    init_model_task(0);
    sched_add_client(&gScheduler, &client, &model_thread_queue);
    while (1) {
        msg = queue_recv(&model_thread_queue);
        do {
            if (((OSScMsg *)msg)->type == OS_SC_RETRACE_MSG || ((OSScMsg *)msg)->type == OS_SC_PRE_NMI_MSG) {}
        } while (queue_try_recv(&model_thread_queue, &msg));
        if (ControllerUpdate() && modelrun) {
            model_iteration();
        }
    }
}
