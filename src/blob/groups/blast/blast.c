/* blast.c: explosions: the blast sprite, a car's fireball and its shock ring (MAT3 visuals.c, StartBlast(),
 * AnimateBlast(), StartNewBlast(), StartNewBlastRing()).
 *
 * StartBlast is the entry point (a car blowing up, a mine, a shot hitting): an animated explosion sprite, which for
 * a car sets the car's fireball off three frames in (StartNewBlast: four flame balls, a glow and the flames of
 * StartSmoke) and spreads a shock ring (StartNewBlastRing). Each runs as an environment visual (GrabEnvEntry,
 * AddToEnvList) whose animator (AnimateBlast here, AnimateNewBlast, AnimateNewBlastRing) runs once a frame until it
 * ends. The fireball and the ring keep their state per car in pc_blasts and pc_blast_rings. */
#include "game/game.h"

typedef union tRGBA { unsigned char c[4]; unsigned int rgba; } tRGBA;
typedef struct tUVMatrix { float uvs[3][3]; float pos[3]; } tUVMatrix;
/* One of a fireball's four flame balls (MAT3 tBlastObj; PC: struct pc_blast_obj). */
typedef struct tBlastObj {
    int mbIdx; tUVMatrix mat; float scale; float scaleInc; float yaw; float yawInc; float pitch; float pitchInc;
    unsigned char alpha; unsigned char alphaInc; signed short unused; float ht;
} tBlastObj;

/* A visual (MAT3 tVisual; the offsets are the N64's). PC: 0x20 bytes: next and func are host pointers, data a u32
 * (docs/port/CARS.md). */
typedef struct tVisual {
    struct tVisual *next;       /* 0x00 */
    s16 index;                  /* 0x04 the blast sprite's frame */
    s16 objnum;                 /* 0x06 its scene object */
    s16 slot;                   /* 0x08 the car, -1 none */
    u8 pad0A[2];
    u32 data;                   /* 0x0C */
    float timeStamp;            /* 0x10 */
    void (*func)(struct tVisual *, s16);   /* 0x14 */
} tVisual;

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ObjInstance { unsigned int flags, culling; tUVMatrix *mat; float scale; float w; u16 index; short child, next, numLights; void *lights, *texture[4], *tile, *tileSize, *palette; unsigned int color; unsigned int envcolor; } ObjInstance;

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

/* On the PC the blasts and their rings are pc_blasts and pc_blast_rings, PC_MAX_LINKS long, with no N64 address
 * (pc_game_types.h, docs/port/DECOUPLE.md 6), with these views' MAT3 field names. */
#define gBlastObjs pc_blasts
#define gBlastRingObjs pc_blast_rings

VIEW(ObjInstance[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

VIEW(float[4][3], unitmatrix);
#define unitmatrix unitmatrix_view

tVisual *GrabEnvEntry(void);

/* Set car `slot`'s fireball off: four flame balls (gObjList[158..161]) 3 ft under the car, starting at 0.4 of
 * their size and growing, turning at random and rising (AnimateNewBlast); with 8 MB or on the later tracks, and with
 * fewer than four players, a glow (gObjList[162]) and five flames (StartSmoke SMOKE_FIRE) as well. The fireball
 * lasts 0.7 s. */
void StartNewBlast(s16 slot)
{
    tVisual *v;
    int i;
    tUVMatrix mat;      /* unused: kept, as it shapes the function's stack frame */
    tRGBA clr = { { 255, 255, 255, 255 } };
    v = GrabEnvEntry();
    if (v) {
        v->func = AnimateNewBlast;
        v->timeStamp = gModelTime;
        v->slot = slot;
        clr.c[0] = 244;
        clr.c[1] = 205;
        clr.c[2] = 20;
        for (i = 0; i < 4; i++) {
            tBlastObj *b;
            b = &gBlastObjs[slot].blastObjs[i];
            if (b->mbIdx != -1) ZOID_RemoveObject(b->mbIdx);
            CopyUV(unitmatrix, b->mat.uvs);
            b->mat.pos[0] = PC_CAR_POS(slot)[0];
            b->mat.pos[1] = PC_CAR_POS(slot)[1] - 3.0f;
            b->mat.pos[2] = PC_CAR_POS(slot)[2];
            b->mbIdx = MBOX_NewObject(gObjList[158 + i], &b->mat, -1, 0x42000);
            b->scale = 0.4f;
            b->scaleInc = 0.05f + Random(0.005f);
            b->mat.uvs[0][0] = b->mat.uvs[1][1] = b->mat.uvs[2][2] = b->scale;
            b->yawInc = 0.1f - Random(0.2f);
            b->pitchInc = 0.1f - Random(0.2f);
            b->ht = 0.0f;
            b->yaw = 0.0f;
            b->pitch = 0.0f;
            b->alpha = 128;     /* translucent, yellow-orange */
            clr.c[3] = b->alpha;
            b->alphaInc = (int)Random(10.0f) + 25;
            ZOID_SetColor(b->mbIdx, &clr);
        }
        if (gNumPlayers < 4 && (expansion_pak_present || gTrackNum >= 6)) {
            CopyUV(unitmatrix, gBlastObjs[slot].mat.uvs);
            gBlastObjs[slot].mat.pos[0] = PC_CAR_POS(slot)[0];
            gBlastObjs[slot].mat.pos[1] = PC_CAR_POS(slot)[1];
            gBlastObjs[slot].mat.pos[2] = PC_CAR_POS(slot)[2];
            if (gBlastObjs[slot].glowIdx != -1) ZOID_RemoveObject(gBlastObjs[slot].glowIdx);
            gBlastObjs[slot].glowIdx = MBOX_NewObject(gObjList[162], &gBlastObjs[slot].mat, -1, 0x52000);
            gBlastObjs[slot].alpha = 95;
            gBlastObjs[slot].alphaCnt = 0;
            ObjInstanceList[gBlastObjs[slot].glowIdx].scale = 0.2f;
            ObjInstanceList[gBlastObjs[slot].glowIdx].w = 1.4f;
            clr.c[0] = 255;
            clr.c[1] = 255;
            clr.c[2] = 255;
            clr.c[3] = gBlastObjs[slot].alpha;
            ZOID_SetColor(gBlastObjs[slot].glowIdx, &clr);
        }
        gBlastObjs[slot].lifeTime = 0.7f;
        gBlastObjs[slot].incTime = 0.0333333f;
        AddToEnvList(v);
    }
    if (gNumPlayers < 4 && (expansion_pak_present || gTrackNum >= 6)) StartSmoke(slot, SMOKE_FIRE, 0, 0);
}

/* Spread a shock ring (gObjList[163]) from car `slot`: white, opaque; AnimateNewBlastRing widens and fades it.
 * (The N64 compiled it into StartBlast.) */
static void StartNewBlastRing(s16 slot)
{
    tVisual *v;
    int i;
    tUVMatrix mat;      /* unused: kept, as it shapes the function's stack frame */
    tRGBA clr = { { 255, 255, 255, 255 } };
    v = GrabEnvEntry();
    if (v) {
        v->func = AnimateNewBlastRing;
        v->timeStamp = gModelTime;
        v->slot = slot;
        CopyUV(unitmatrix, gBlastRingObjs[slot].mat.uvs);
        gBlastRingObjs[slot].mat.pos[0] = PC_CAR_POS(slot)[0];
        gBlastRingObjs[slot].mat.pos[1] = PC_CAR_POS(slot)[1];
        gBlastRingObjs[slot].mat.pos[2] = PC_CAR_POS(slot)[2];
        if (gBlastRingObjs[slot].ringIdx != -1) ZOID_RemoveObject(gBlastRingObjs[slot].ringIdx);
        gBlastRingObjs[slot].ringIdx = MBOX_NewObject(gObjList[163], &gBlastRingObjs[slot].mat, -1, 0x42000);
        gBlastRingObjs[slot].ralpha = 240;
        gBlastRingObjs[slot].ralphaCnt = 0;
        ZOID_SetColor(gBlastRingObjs[slot].ringIdx, &clr);
        gBlastRingObjs[slot].incTime = 0.0333333f;
        AddToEnvList(v);
    }
}

/* game.frame_rate 60 (docs/port/FRAMERATE.md 7): a blast on no car steps its animation once a frame, the N64's 30 a
 * second: at 60 it steps on the frames that complete a 30 Hz frame (pc_game_fps.h). */
#define PC_EACH30_IF(c) ((c) && (!pc_fps60 || pc_frame_steps))
/* The blast sprite's animator (a visual's func): `op` 0 removes it. Its 30 frames (gObjList[164..193]) step
 * every 1/30 s; a blast on a car follows the car while it is crashed (and ends with the crash), sets the car's
 * fireball off at frame 3 (StartNewBlast) and, with 8 MB or on the later tracks and fewer than four players, scorches
 * the world under it at frame 4 (newWorldDamage). Nothing moves while the game is paused. */
void AnimateBlast(tVisual *v, s16 op)
{
    if (gPauseState) return;
    if (!op) {
release:
        ZOID_RemoveObject(v->objnum);
        v->objnum = -1;
        ReleaseEnviron(v, 1);
        return;
    }
    if (v->slot >= 0) {
        if (!PC_MODEL_CRASHFLAG(v->slot)) goto release;
        ObjInstanceList[v->objnum].mat->pos[0] = PC_CAR_POS(v->slot)[0];
        ObjInstanceList[v->objnum].mat->pos[1] = PC_CAR_POS(v->slot)[1];
        ObjInstanceList[v->objnum].mat->pos[2] = PC_CAR_POS(v->slot)[2];
    }
    v->timeStamp -= gFrameSeconds;
    if (PC_EACH30_IF(v->slot < 0) || (v->slot >= 0 && v->timeStamp <= 0.0f)) {
        v->timeStamp = 0.0333333f;
        if (v->index == 30) goto release;
        ZOID_SetObjectDef(v->objnum, gObjList[164 + v->index]);
        v->index++;
        if (v->slot >= 0) {
            if (gNumPlayers < 4 && (expansion_pak_present || gTrackNum >= 6) && v->index == 4) newWorldDamage(PC_CAR_POS(v->slot), v->slot);
            if (v->index == 3) StartNewBlast(v->slot);
        }
    }
}

/* Start an explosion: on car *(s16 *)data if linkToCar (with its shock ring, under four players), else at the
 * position (float[3]) data points to. `scale` is the sprite's size (1 a car's, 0.5 a shot's, 0.4 a mine's) and picks
 * the sound when playSound is set (big, medium or small). The sprite's matrix is the next of the 50 kept in
 * gBlastMats, round robin. With 4 MB on the first six tracks there is no sprite: only the car's fireball. */
void StartBlast(void *data, int linkToCar, float scale, int playSound)
{
    tVisual *v;
    int snd;
    tUVMatrix *mat;
    if (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6) {
        StartNewBlast(*(s16 *)data);
        return;
    }
    if (linkToCar && gNumPlayers < 4) StartNewBlastRing(*(s16 *)data);
    v = GrabEnvEntry();
    if (!v) return;
    mat = gBlastMats[gBlastMatCnt].mat;
    CopyUV(unitmatrix, mat->uvs);
    v->func = AnimateBlast;
    v->timeStamp = 0.0333333f;
    if (linkToCar) {
        s16 slot;
        slot = *(s16 *)data;
        v->slot = slot;
        mat->pos[0] = PC_CAR_POS(v->slot)[0];
        mat->pos[1] = PC_CAR_POS(v->slot)[1];
        mat->pos[2] = PC_CAR_POS(v->slot)[2];
    } else {
        v->slot = -1;
        mat->pos[0] = ((float *)data)[0];
        mat->pos[1] = ((float *)data)[1];
        mat->pos[2] = ((float *)data)[2];
    }
    v->index = 0;
    v->objnum = MBOX_NewObject(gObjList[164], mat, -1, 0x50000);
    AddToEnvList(v);
    ObjInstanceList[v->objnum].scale = scale;
    if (++gBlastMatCnt >= 50) gBlastMatCnt = 0;
    if (playSound) {
        snd = scale >= 1.0f ? 45 : scale >= 0.5f ? 69 : 47;     /* the big, medium and small explosion sounds */
        EmitGameSound(mat->pos, (int)unitmatrix[3], 400.0f, 0.0f, 1.0f, 0.0f, snd, v->objnum, 0, 128);
    }
}
