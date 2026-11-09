/* AnimateNewBlast.c: a car's fireball, frame by frame (MAT3 visuals.c, AnimateNewBlast()).
 *
 * StartNewBlast (groups/blast) sets the fireball off when a car blows up: four flame balls that grow, turn and
 * rise, and a flickering glow. This, its visual's animator, runs once a frame until the car is no longer crashed.
 * The fireball's state is the car's pc_blasts entry. */
#include "game/game.h"

typedef union tRGBA { unsigned char c[4]; unsigned int rgba; } tRGBA;
typedef struct tUVMatrix { float uvs[3][3]; float pos[3]; } tUVMatrix;
/* One of the four flame balls (MAT3 tBlastObj; PC: struct pc_blast_obj). */
typedef struct tBlastObj {
    int mbIdx;                  /* 0x00 its scene object, -1 none */
    tUVMatrix mat;              /* 0x04 its matrix: its scene node points here */
    float scale;                /* 0x34 its size */
    float scaleInc;             /* 0x38 its growth a step (slowing by 0.002 a step) */
    float yaw;                  /* 0x3C its turn, radians */
    float yawInc;               /* 0x40 */
    float pitch;                /* 0x44 */
    float pitchInc;             /* 0x48 */
    unsigned char alpha;        /* 0x4C */
    unsigned char alphaInc;     /* 0x4D its fade a step at the end */
    signed short unused;        /* 0x4E */
    float ht;                   /* 0x50 its height above the start, feet (and its spread from the middle) */
} tBlastObj;

/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct tVisual { void *next; u8 pad4[2]; s16 objnum; s16 slot; u8 padA[0x14 - 0xA]; void *func; } tVisual;

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ObjInstance { unsigned int flags, culling; void *mat; float scale; float w; unsigned short index; short child, next, numLights; void *lights, *texture[4], *tile, *tileSize, *palette; unsigned int color; unsigned int envcolor; } ObjInstance;

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
/* On the PC the blasts are pc_blasts, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6), with this view's MAT3 field names. */
#define gBlastObjs pc_blasts
VIEW(ObjInstance[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* The fireball's animator (a visual's func): `op` 0 removes it. The flame balls go when the fireball's 0.7 s run
 * out or the car starts its respawn; everything goes, and the visual ends, when the car is no longer crashed.
 * Every 1/30 s the fireball follows the car: the glow flickers (alpha 95, 64, 95, then held for up to two more
 * steps) and grows to its full size, and each flame ball grows, turns and rises 0.2 ft, the last three spreading
 * from the first along the diagonals; in the last 0.2 s they fade. Nothing moves while the game is paused. (The N64
 * draws the glow only with 8 MB or on the later tracks, and with fewer than four players: StartNewBlast.) */
void AnimateNewBlast(tVisual *v, s16 op)
{
    int i;
    tBlastObj *b;
    tRGBA clr;
    float x;
    float y;
    if (gPauseState) return;
    gBlastObjs[v->slot].lifeTime -= gFrameSeconds;
    if (gBlastObjs[v->slot].lifeTime <= 0.0f || PC_MODEL_MOVING_STATE(v->slot) >= 0) {
        for (i = 0; i < 4; i++) {
            b = &gBlastObjs[v->slot].blastObjs[i];
            if (b->mbIdx != -1) ZOID_RemoveObject(b->mbIdx);
            b->mbIdx = -1;
        }
    }
    if (!PC_MODEL_CRASHFLAG(v->slot)) {
        if (expansion_pak_present || gTrackNum >= 6) {
            if (gBlastObjs[v->slot].glowIdx != -1) {
                ZOID_RemoveObject(gBlastObjs[v->slot].glowIdx);
                gBlastObjs[v->slot].glowIdx = -1;
            }
        }
        for (i = 0; i < 4; i++) {
            b = &gBlastObjs[v->slot].blastObjs[i];
            if (b->mbIdx != -1) ZOID_RemoveObject(b->mbIdx);
            b->mbIdx = -1;
        }
        goto release;
    }
    if (!op) {
release:
        ReleaseEnviron(v, 1);
        return;
    }
    gBlastObjs[v->slot].mat.pos[0] = PC_CAR_POS(v->slot)[0];
    gBlastObjs[v->slot].mat.pos[1] = PC_CAR_POS(v->slot)[1];
    gBlastObjs[v->slot].mat.pos[2] = PC_CAR_POS(v->slot)[2];
    gBlastObjs[v->slot].incTime -= gFrameSeconds;
    if (gBlastObjs[v->slot].incTime <= 0.0f) {
        if (gBlastObjs[v->slot].alphaCnt == 0) gBlastObjs[v->slot].alpha -= 31;
        else if (gBlastObjs[v->slot].alphaCnt == 1) gBlastObjs[v->slot].alpha += 31;
        else if (gBlastObjs[v->slot].alphaCnt > 1) {
            gBlastObjs[v->slot].alphaCnt = (int)Random(3.0f) * -1;
            gBlastObjs[v->slot].alpha = 95;
        }
        gBlastObjs[v->slot].alphaCnt++;
        if (gNumPlayers < 4 && (expansion_pak_present || gTrackNum >= 6)) {
            clr.c[0] = 255;
            clr.c[1] = 255;
            clr.c[2] = 255;
            clr.c[3] = gBlastObjs[v->slot].alpha;
            ZOID_SetColor(gBlastObjs[v->slot].glowIdx, &clr);
            if (ObjInstanceList[gBlastObjs[v->slot].glowIdx].scale < 1.0f)
                ObjInstanceList[gBlastObjs[v->slot].glowIdx].scale += 0.05f;
        }
        for (i = 0; i < 4; i++) {
            b = &gBlastObjs[v->slot].blastObjs[i];
            if (b->mbIdx == -1) continue;
            b->mat.uvs[0][0] = b->mat.uvs[1][1] = b->mat.uvs[2][2] = b->scale;
            b->mat.uvs[0][1] = b->mat.uvs[1][0] = b->mat.uvs[2][0] = 0.0f;
            b->mat.uvs[0][2] = b->mat.uvs[1][2] = b->mat.uvs[2][1] = 0.0f;
            switch (i) {
                case 0: x = 0.0f; y = 0.0f; break;
                case 1: x = 0.5f * b->ht; y = 0.5f * b->ht; break;
                case 2: x = -0.5f * b->ht; y = -0.5f * b->ht; break;
                case 3: x = -0.5f * b->ht; y = 0.5f * b->ht; break;
            }
            b->mat.pos[0] = x + PC_CAR_POS(v->slot)[0];
            b->mat.pos[2] = y + PC_CAR_POS(v->slot)[2];
            b->mat.pos[1] = b->ht + PC_CAR_POS(v->slot)[1] - 3.0f;
            b->ht += 0.2f;
            PitchUV(b->pitch, b->mat.uvs);
            YawUV(b->yaw, b->mat.uvs);
            b->scale += b->scaleInc;
            b->scaleInc -= 0.002f;
            b->yaw += b->yawInc;
            b->pitch += b->pitchInc;
            if (gBlastObjs[v->slot].lifeTime < 0.2f) {
                clr.c[0] = 244;
                clr.c[1] = 205;
                clr.c[2] = 20;
                clr.c[3] = b->alpha;
                if (b->alpha > b->alphaInc) b->alpha -= b->alphaInc;
                ZOID_SetColor(b->mbIdx, &clr);
            }
        }
        gBlastObjs[v->slot].incTime = 0.0333333f;
    }
}
