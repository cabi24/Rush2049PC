/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* N64 frame dispatch. The VI-derived frame-delta clock is observed through the
 * same volatile view supported by repeated native loads in AnimateRotGeneric.
 * Its writers are InitializeTimer and UpdateFrameTimer; see the companion proof.
 * The original N64 header is not recovered. No arcade donor is claimed.
 * hud_obj_angle_speed is the external protected single-precision pi constant.
 * The overlay mode must be reread after animCoins may change it.
 */
typedef int s32;
typedef unsigned int u32;
typedef float f32;
extern s32 gGameState, gPauseState, gRace;
extern f32 hud_obj_angle_speed, hudObjAngle;
extern volatile f32 gFrameSeconds;
extern void animCoins(s32);
extern void UpdateHUDExtraPostCamera(void);
void UpdateHUDPostCamera(void) {
    if (gGameState != -1 && ((gGameState & 0x200000) || (gGameState & 0x400000))) {
        if (!gPauseState) hudObjAngle += hud_obj_angle_speed * gFrameSeconds;
        if (gRace == 0 || gRace == 1 || gRace == 2 || gRace == 3 || gRace == 4) animCoins(0);
        if (gRace == 6 || gRace == 4) UpdateHUDExtraPostCamera();
    }
}
