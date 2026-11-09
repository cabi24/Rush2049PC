/* InitPathData: the paths at a track's load (MAT3 maxpath.c). */
#include "game/game.h"

/* Reads the track's paths from its path file and clears maxpath's controls (MAT3 lastwheel, lastbrake and
 * lastthrottle: the PC's pc_mp_steer, pc_mp_brake and pc_mp_gas, one per car slot, all of them cleared here where
 * MAT3 clears its six) and its drone switching. */
void InitPathData(void)
{
    s16 i;
    ReadPathData();
    for (i = 0; i < PC_MAX_LINKS; i++) {
        PC_MP_STEER(i) = 0.0f;
        PC_MP_BRAKE(i) = 0.0f;
        PC_MP_GAS(i) = 0.0f;
    }
    gLastPathSwitchTime = gModelTime;
    current_drone = -1;
}
