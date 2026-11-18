#include "game/game.h"

/* A battle power-up's icon (MAT3 targets.c _AnimateWeaponIcon), the environment visual of a weapon target: while
 * the target is shown (Flags 2) it spins by its spin (its data: f32[3] radians a second about x, y, z); while hidden
 * it shows again once no player's car has the weapon its object (type) stands for. The visual's data is the
 * target. */
void AnimateWeaponIcon(struct pc_visual *v, s16 op)
{
    tTarget *t;
    f32 *angV;
    s32 pad;
    f32 angVF[3];
    s32 i;
    s32 ok;
    s32 wep;
    if (!op) {
        ReleaseEnviron(v, 1);
        return;
    }
    if (gPauseState != 0) return;
    t = (tTarget *) v->data;
    if (t->Flags & 2) {
        angV = t->data;
        angVF[0] = angV[0] * gFrameSeconds;
        angVF[1] = angV[1] * gFrameSeconds;
        angVF[2] = angV[2] * gFrameSeconds;
        RotateUV(angVF, t->mat.uvs);
    } else if (ZOID_ObjectFlags(t->dlIdx) & 0x80000000) {
        switch (t->type) {
        case 350: wep = 0; break;
        case 351: wep = 1; break;
        case 352: wep = 2; break;
        case 355: wep = 3; break;
        case 356: wep = 4; break;
        case 357: wep = 5; break;
        case 358: wep = 6; break;
        case 360: wep = 7; break;
        default: wep = -1; break;
        }
        if (wep != -1) {
            ok = 1;
            for (i = 0; i < gNumPlayers; i++) {
                if (wep == PC_CAR_WEAPON_TYPE(i)) ok = 0;
            }
            if (ok) {
                MBOX_ShowObject(t->dlIdx, 0, 15);
                t->Flags |= 2;
            }
        }
    } else {
        MBOX_HideObject(t->dlIdx, 0, 15);
    }
}
