/* ZeroCamera: a camera back to the origin (MAT3 camera.c). */
#include "game/game.h"

VIEW(s32, unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(tCamera[], gCamera);
#define gCamera gCamera_view

/* Resets the camera of window bit `camera` (1, 2, 4 or 8: camera 0 to 3): at the origin, looking down the axes,
 * with its spring at rest. */
void ZeroCamera(s16 camera)
{
    s32 i = ((camera & 1) ? 0 : (camera & 2) ? 1 : (camera & 4) ? 2 : 3);
    gCamera[i].slot = i;
    gCamera[i].mat.pos[0] = 0.0f; gCamera[i].mat.pos[1] = 0.0f; gCamera[i].mat.pos[2] = 0.0f;
    CopyUV(&unitmatrix, &gCamera[i]);
    gCamera[i].view2spring = 0.0f;
}
