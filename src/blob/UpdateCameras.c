/* UpdateCameras: the cameras ready for drawing, each frame (MAT3 camera.c). */
#include "game/game.h"

VIEW(u16, gNumWindows);
#define gNumWindows gNumWindows_view


/* For each window's camera: the matrix the N64 draws by (n64: mat with four elements negated, its handedness), and
 * the sky for it but while paused; then the HUD's camera-dependent parts and the countdown. */
void UpdateCameras(void)
{
    s32 i;
    tCamera *cam;
    UVMat *n64;

    for (i = 0; i < (s16) gNumWindows; i++) {
        cam = &gCamera[i];
        n64 = &cam->n64;
        cam->n64.pos[0] = cam->mat.pos[0];
        cam->n64.pos[1] = cam->mat.pos[1];
        cam->n64.pos[2] = cam->mat.pos[2];
        CopyUV(cam, n64);
        cam->n64.uvs[0][1] = -cam->n64.uvs[0][1];
        cam->n64.uvs[0][2] = -cam->n64.uvs[0][2];
        cam->n64.uvs[1][0] = -cam->n64.uvs[1][0];
        cam->n64.uvs[2][0] = -cam->n64.uvs[2][0];
        if (gPauseState == 0) {
            SkyUpdate(i);
        }
    }
    UpdateHUDPostCamera();
    ShowCountdown(gCountDown > 0);
}
