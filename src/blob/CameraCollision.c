/* CameraCollision: a camera kept out of the walls (MAT3 stree.c). */
#include "game/game.h"

typedef struct Surface Surface;

/* Traces from `lookat` (what the camera looks at) to the camera's position `ipos` through the track's surfaces
 * (ShotToWorldCollision, half a foot wide, skipping kind 6); on a hit `ipos` is the hit point, moved a quarter foot
 * back towards `lookat`. Returns NULL always. */
Surface *CameraCollision(float *ipos, float *lookat)
{
    Surface *surfaceptr;
    float surfuvs[3][3];
    float temp[3];

    surfaceptr = ShotToWorldCollision(lookat, ipos, surfuvs, 0.5f, 1, 6);
    if (surfaceptr) {
        temp[0] = lookat[0] - ipos[0];
        temp[1] = lookat[1] - ipos[1];
        temp[2] = lookat[2] - ipos[2];
        NormalVector(temp);
        ipos[0] = ipos[0] + 0.25f * temp[0];
        ipos[1] = ipos[1] + 0.25f * temp[1];
        ipos[2] = ipos[2] + 0.25f * temp[2];
    }
    return (Surface *)0;
}
