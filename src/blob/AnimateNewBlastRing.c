/* AnimateNewBlastRing.c: widen and fade a blast's shock ring (MAT3 visuals.c, AnimateNewBlastRing()).
 *
 * StartNewBlastRing (groups/blast) starts the ring around an exploding car; this, its visual's animator, runs once a
 * frame until the ring is gone. The ring's state is the car's pc_blast_rings entry. */
#include "game/game.h"

/* On the PC the blast rings are pc_blast_rings, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6). */
#define gBlastRingObjs pc_blast_rings

/* The ring's animator (a visual's func): `op` 0 removes it. While its car is crashed the ring stays on the car
 * and, every 1/30 s, its scale grows by 1.5 (from 1); from its 8th step it fades (alpha down 24 a step) and after
 * its 16th it is removed. It ends at once when the car is no longer crashed. Nothing moves while the game is
 * paused. */
void AnimateNewBlastRing(tVisual *v,s16 op) {
    tVec3 pos;          /* unused: kept, as it shapes the function's stack frame */
    s32 phase;          /* the step count, then (below) the ring's scene node */
    tRGBA clr;
    if(gPauseState) return;
    if(!pc_models[v->slot].crashflag) {
        if(gBlastRingObjs[v->slot].ringIdx!=-1) {
            RemoveObjectTree(gBlastRingObjs[v->slot].ringIdx,0,0);
            gBlastRingObjs[v->slot].ringIdx=-1;
        }
        goto release;
    }
    if(!op) {
release:
        ReleaseEnviron(v,1);
        return;
    }
    gBlastRingObjs[v->slot].mat.pos[0]=pc_cars[v->slot].dr_pos[0];
    gBlastRingObjs[v->slot].mat.pos[1]=pc_cars[v->slot].dr_pos[1];
    gBlastRingObjs[v->slot].mat.pos[2]=pc_cars[v->slot].dr_pos[2];
    gBlastRingObjs[v->slot].incTime-=gFrameSeconds;
    if(gBlastRingObjs[v->slot].incTime<=0.0f) {
        gBlastRingObjs[v->slot].mat.uvs[0][0]+=1.5f;
        gBlastRingObjs[v->slot].mat.uvs[1][1]+=1.5f;
        gBlastRingObjs[v->slot].mat.uvs[2][2]+=1.5f;
        phase=gBlastRingObjs[v->slot].ralphaCnt;
        if(phase>=16) {
            if(gBlastRingObjs[v->slot].ringIdx!=-1) {
                RemoveObjectTree(gBlastRingObjs[v->slot].ringIdx,0,0);
                gBlastRingObjs[v->slot].ringIdx=-1;
            }
            goto release;
        }
        if(phase>=7) {
            /* ZOID_SetColor, inlined */
            clr.c[0]=255;
            clr.c[1]=255;
            clr.c[2]=255;
            clr.c[3]=gBlastRingObjs[v->slot].ralpha;
            phase=gBlastRingObjs[v->slot].ringNode;
            ObjInstanceList[phase].color.rgba=clr.rgba;
            if(gBlastRingObjs[v->slot].ralpha>=25) {
                gBlastRingObjs[v->slot].ralpha-=24;
            }
        }
        gBlastRingObjs[v->slot].ralphaCnt++;
        gBlastRingObjs[v->slot].incTime=0.0333333f;
    }
    return;
}
