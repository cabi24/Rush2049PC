/* HAL_CallOncePerFrameFuncs (MAT3 HAL_time.c): calls the once-a-frame functions (hal_time/group.c has the table). */
#include "game/game.h"

extern intptr_t gOPPFuncs[10];   /* function pointers (groups/hal_time) */

/* Calls each function in the table, in order (UpdateFrame, once a frame). */
void HAL_CallOncePerFrameFuncs(void)
{
    void (**p)(void);

    if (gInited_HALTime == 0) {
        HAL_InitTime();
    }
    p = (void (**)(void)) &gOPPFuncs;
    do {
        if (*p != 0) {
            (*p)();
        }
        p++;
    } while (p != (void (**)(void)) (gOPPFuncs + 10));
}
