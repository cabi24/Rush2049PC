/* HAL_CallOncePerFrameFuncs (MAT3 HAL_time.c): calls the once-a-frame functions (hal_time/group.c has the table). */
#include "game/game.h"

extern void (*gOPPFuncs[10])(void);   /* the functions (groups/hal_time) */

/* Calls each function in the table, in order (UpdateFrame, once a frame). */
void HAL_CallOncePerFrameFuncs(void)
{
    void (**p)(void);

    if (gInited_HALTime == 0) {
        HAL_InitTime();
    }
    p = gOPPFuncs;
    do {
        if (*p != NULL) {
            (*p)();
        }
        p++;
    } while (p != gOPPFuncs + 10);
}
