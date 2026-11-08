/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * visuals.c blast trio, MAT3 GameSource/src/visuals.c (builds on groups/w36l_blast, f308).
 *   StartNewBlast         = StartNewBlast (GC 0x80057998, static; internal here: slot read from the caller's 0(sp)
 *                           home, s0-s8 clobbered unsaved)
 *   AnimateBlast = AnimateBlast  (GC 0x800587C0, static; the tVisual callback StartBlast installs)
 *   StartBlast       = StartBlast    (GC 0x800585E4; StartNewBlastRing inlined)
 * All three MATCH in this group (the scorer) and are EQUAL in the whole-program unit.
 * N64 differences from MAT3: StartNewBlast's loop works through `tBlastObj *b = &gBlastObjs[slot].blastObjs[i]`
 * for every field and sets clr.c[3] from b->alpha after `b->alpha = 128` (MAT3: `clr.c[3] = 128` earlier); alphaInc
 * Random(10)+25 (MAT3 +17); glow alpha 95 (MAT3 56); the glow and StartSmoke (StartSmoke) need
 * gNumPlayers < 4 and (expansion_pak_present || gTrackNum >= 6). StartBlast runs StartNewBlast directly for tracks 0..5
 * without expansion_pak_present; the ring needs gNumPlayers < 4; StartNewBlastRing tests `if (v) {...}` (MAT3: early
 * return); the if-arm copies the slot through an s16 local; sound ids 45/69/47 through a `snd` local.
 * Why these forms (traced, RESULTS): a store through the pointer variable b kills the rand seed in uopt (no PRE
 * of the next Random's seed load) although uopt then folds b into the IV (s0-relative stores as in retail); the byte
 * load of `clr.c[3] = b->alpha` takes one ring temp, so the 4th Random reloads the seed into t6 (`sw t9,0(s1);
 * lw t6,0(s1)`); CopyUV through b decides the s2/s3 colouring tie. In StartBlast the ring's early return stopped uopt
 * from carrying MBOX_NewObject's -1/gObjList/0x50000 argument webs over `if (linkToCar)` (retail sets a0..a3 before
 * the branch and multiplies by 952 with shifts); the s16 local gives retail's `lh v0,0(t9)`.
 * Helpers: Random = Random over libc rand (game_rand); ZOID_RemoveObject -> ZOID_RemoveObjectTree ->
 * _RemoveObjectTree (RemoveObjectTree) as in GC zoid.c; ZOID_SetColor = ZOID_SetColor; MBOX_NewObject wraps
 * ZOID_AddObject (ZOID_AddObject); GrabEnvEntry = GrabEnvEntry; AddToEnvList inline; ReleaseEnviron =
 * ReleaseEnviron; ZOID_SetObjectDef = ZOID_SetObjectDef and EmitGameSound = EmitGameSound (both locked
 * singles, byte-identical context copies, inlined by umerge); AudEmitEffect = AudEmitEffect; newWorldDamage =
 * newWorldDamage (image B); StartSmoke = StartSmoke.
 * Unused locals (the exact frame residual, disclosed): StartNewBlast and the inlined StartNewBlastRing share the local
 * template `tVisual *v; int i; tUVMatrix mat; tRGBA clr;` with `mat` unused in both and `i` unused in the ring.
 *   StartNewBlast: retail frame 272, v at sp+268 (-4), clr at sp+212 (-60); without `mat` the frame is 224 and 11 words
 *   differ (12 slots).
 *   StartBlast: retail frame 352, ring v at sp+324 (-28), ring clr at sp+268 (-84). Without the ring's `mat` the frame
 *   is 304 (32 words differ); without the ring's `i`, 2 words differ (clr slot); the ring's `int u0, u1` after clr are
 *   2 more slots: without them the frame is 344 (34 words differ: named slots match top-relative, the spill homes at
 *   sp+144/148 bottom-relative, so the local area is 8 B short).
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;

typedef union tRGBA { unsigned char c[4]; unsigned int rgba; } tRGBA;
typedef struct tUVMatrix { float uvs[3][3]; float pos[3]; } tUVMatrix;
typedef struct tBlastObj {
    int mbIdx; tUVMatrix mat; float scale; float scaleInc; float yaw; float yawInc; float pitch; float pitchInc;
    unsigned char alpha; unsigned char alphaInc; signed short unused; float ht;
} tBlastObj;
typedef struct tBlastStruct {
    tBlastObj blastObjs[4]; int glowIdx; tUVMatrix mat;
    unsigned char alpha; unsigned char alphaCnt; unsigned char ralpha; unsigned char ralphaCnt;
    float lifeTime; float incTime;
} tBlastStruct;
typedef struct tBlastRingStruct { int ringIdx; tUVMatrix mat; unsigned char ralpha; unsigned char ralphaCnt; signed short unused; float incTime; } tBlastRingStruct;
typedef struct tBlastMat { tUVMatrix *mat; } tBlastMat;
typedef struct tVisual {
    struct tVisual *next;       /* 0x00 */
    s16 index;                  /* 0x04 */
    s16 objnum;                 /* 0x06 */
    s16 slot;                   /* 0x08 */
    u8 pad0A[2];
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
    u32 data;                   /* 0x0C */
    float timeStamp;            /* 0x10 */
    void (*func)(struct tVisual *, s16);   /* 0x14 */
} tVisual;
typedef struct tCarData { u8 pad0[8]; float dr_pos[3]; u8 pad14[0x110 - 0x14]; struct pc_visual visuals[21]; u8 pad14_308[0x380 - 0x308]; void *input; u8 pad14_384[0x3B8 - 0x384]; } tCarData;
typedef struct tModelData { u8 pad0[0x640]; s8 crashflag; u8 pad641[0x808 - 0x641]; } tModelData;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct ObjInstance { unsigned int flags, w4; tUVMatrix *mat; float scale; float w; u16 index; short child, next, mode; void *light, *tex[4], *rect, *clip, *pal; unsigned int color; unsigned int envcolor; } ObjInstance;

extern s32 gPauseState;                  /* gPauseState */
extern volatile f32 gFrameSeconds;         /* gFrameSeconds */
extern f32 gModelTime;                  /* gModelTime */
/* On the PC the blasts and their rings are pc_blasts and pc_blast_rings, PC_MAX_LINKS long, with no N64 address
 * (pc_game_types.h, docs/port/DECOUPLE.md 6), with these views' MAT3 field names. */
#define gBlastObjs pc_blasts
#define gBlastRingObjs pc_blast_rings
extern tBlastMat gBlastMats[];          /* gBlastMats */
extern s16 gBlastMatCnt;                  /* gBlastMatCnt */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern ObjInstance ObjInstanceList[];        /* ObjInstanceList */
extern u16 gObjList[];                /* gObjList */
extern float unitmatrix[4][3];          /* unitmatrix */
extern tVisual *gEnvList;             /* gEnvList */
extern s8 expansion_pak_present;
extern s8 gTrackNum;                   /* gTrackNum */
extern s16 gNumPlayers;
extern s8 gGameSoundsEnabled;
extern int rand_seed;

tVisual *GrabEnvEntry(void);                                   /* GrabEnvEntry */
void RemoveObjectTree(s16 objnum, s32 removeChildren, s32 b);  /* _RemoveObjectTree */
void ReleaseEnviron(tVisual *v, s32 unlink);            /* ReleaseEnviron */
void CopyUV(float (*src)[3], float (*dst)[3]);            /* CopyUV */
s32 ZOID_AddObject(s32 objnum, tUVMatrix *mat, s16 parent, s32 flags);   /* MBOX_NewObjectCam? */
void StartSmoke(s16 slot, u32 type, s32 a, f32 *b);      /* StartSmoke */
void newWorldDamage(f32 *pos, s16 slot);                         /* newWorldDamage */
void ZOID_SetObjectDef(s16 objnum, u16 def);                        /* ZOID_SetObjectDef */
int EmitGameSound(const float *pos, intptr_t dir, float fadeDistance, float fadeCompression, float maxVol, float minVol, int fxid, int subId, int replace, unsigned char priority); /* EmitGameSound (locked, kept) */
void AnimateNewBlast(tVisual *v, s16 op);                /* AnimateNewBlast */
void AnimateBlast(tVisual *v, s16 op);
void AnimateNewBlastRing(tVisual *v, s16 op);                         /* AnimateNewBlastRing */

int game_rand(void);

f32 Random(f32 max);

/* The game's own ZOID_RemoveObject, MBOX_NewObject and AddToEnvList (this file had static copies of each: the same
 * code). */
void ZOID_RemoveObject(s16 objnum);

void ZOID_SetColor(s16 objnum, tRGBA *col);

int MBOX_NewObject(int objnum, tUVMatrix *mat, int parent, unsigned int flags);

static void SetObjColor(int objnum, tRGBA *col)
{
    ObjInstanceList[objnum].color = col->rgba;
}

void AddToEnvList(tVisual *v);

/* StartNewBlast */
void StartNewBlast(s16 slot)
{
    tVisual *v;
    int i;
    tUVMatrix mat;
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
            b->alpha = 128;
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
            SetObjColor(gBlastObjs[slot].glowIdx, &clr);
        }
        gBlastObjs[slot].lifeTime = 0.7f;
        gBlastObjs[slot].incTime = 0.0333333f;
        AddToEnvList(v);
    }
    if (gNumPlayers < 4 && (expansion_pak_present || gTrackNum >= 6)) StartSmoke(slot, 5, 0, 0);
}

/* StartNewBlastRing (inlined into StartBlast) */
static void StartNewBlastRing(s16 slot)
{
    tVisual *v;
    int i;
    tUVMatrix mat;
    tRGBA clr = { { 255, 255, 255, 255 } };
    int u0, u1;
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
        SetObjColor(gBlastRingObjs[slot].ringIdx, &clr);
        gBlastRingObjs[slot].incTime = 0.0333333f;
        AddToEnvList(v);
    }
}

/* game.frame_rate 60 (docs/port/FRAMERATE.md 7): a blast on no car steps its animation once a frame, the N64's 30 a
 * second: at 60 it steps on the frames that complete a 30 Hz frame (pc_game_fps.h). */
#define PC_EACH30_IF(c) ((c) && (!pc_fps60 || pc_frame_steps))
/* AnimateBlast */
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

/* StartBlast */
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
        snd = scale >= 1.0f ? 45 : scale >= 0.5f ? 69 : 47;
        EmitGameSound(mat->pos, (int)unitmatrix[3], 400.0f, 0.0f, 1.0f, 0.0f, snd, v->objnum, 0, 128);
    }
}
