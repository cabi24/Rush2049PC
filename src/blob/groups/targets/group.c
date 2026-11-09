/* Targets (MAT3 targets.c): setting up the track's targets, the objects of its world and the path targets of its
 * PTHD records whose names match a kind in targetInfo (SetupTarget), giving each its kind's record, its environment
 * visual and its start (SetTargetDef, SetTargetDefs). The targets are PC data in the host layout (tTarget,
 * pc_target_nodes: their link and their kind's record are pointers; docs/port/POINTERS.md 10.2). The track WAD's
 * PTHD records and a path target's record keep the N64's layout, with 32-bit window addresses (PC_N64W, PC_N64P),
 * until the WAD's records are native (docs/port/NATIVE.md step 7). */
#include "game/game.h"

typedef struct Vec3 { f32 x, y, z; } Vec3;

/* A target path (MAT3 tBasicTargetPath, a PTHD record; targetpaths/group.c has it all). N64 layout. */
typedef struct tBasicTargetPath {
    char name[16];  /* 0x00 */
    u32 flags;      /* 0x10 4 in forward races, 8 in backward races, 0x10 the target doesn't collide, 0x800 its
                     * collision object goes when not in this race's direction, 0x8000 not shown */
    s16 numPts;     /* 0x14 */
    s16 collIdx;    /* 0x16 its collision object, 0 none */
    u32 trigPtr;    /* 0x18 */
    u32 pts;        /* 0x1C its points (TargetPathPt), a window address */
    s32 parent;     /* 0x20 its target's record (a window address), here the key the world's groups know it by */
} tBasicTargetPath;

/* A world object (MAT3 tModelPart), the track WAD's: a target where its name matches a kind's. */
typedef struct tModelPart {
    char name[16];  /* 0x00 */
    UVMat mat;      /* 0x10 its orientation and position */
    u32 dlflags;    /* 0x40 its scene object's flags */
    s32 w68;
    s32 w72;
    s32 tidx;       /* 0x4C the key the world's groups (gTargetGroupList) know it by */
} tModelPart;

/* A mine's record (MAT3 tTargetStructMine, battle's 16 in gTargetStructPtrs[5]): the fields read here. */
typedef struct tTargetStructMine { s32 dlIdx; s32 glowdls[2]; u8 gap12[122]; s16 used; } tTargetStructMine;

/* A path target's record (tTargetStructPath) starts with its path, a PTHD record, as a window address. */
#define STATE_PATH(s) (((tTargetStructPath *) (s))->pathData)
#define PATH_OF(s) ((tBasicTargetPath *) PC_N64P(STATE_PATH(s)))
/* A kind's start function (TargInfo.startFunc, a void * in the table). */
#define START_FUNC(info) ((void (*)(tTarget *)) (info)->startFunc)

extern int strlen(const char *);

/* Defines target `t` once the race's targets are counted: its kind's record (set_target_data), its scene object's
 * object definition, its environment visual (a kind with flag 2: the kind's visFunc animates it, and with flag 4 the
 * start function runs now too), and a kind with flag 0x4000 starts now, or a path target the world placed has its
 * bounding box model put where it stands. */
void SetTargetDef(tTarget *t)
{
    TargInfo *info;
    s32 category;
    s32 resource;
    u16 value;
    struct pc_visual *v;
    Vector transform;
    info=&targetInfo[t->infoIdx];
    category=info->structureID;
    set_target_data(t,info,category);
    if(t->otype!=-1) {
        resource=(s16)t->dlIdx;
        value=gObjList[t->otype];
        ObjInstanceList[resource].index=value;
    }
    if(info->Flags&2) {
        v=GrabEnvEntry();
        if(v) {
            v->index=0;
            v->func=info->visFunc;
            v->data=(uintptr_t)t;           /* the visual animates the target */
            v->timeStamp=0.0f;
            v->next=gEnvList;
            gEnvList=v;
            if((info->Flags&4) && info->startFunc) START_FUNC(info)(t);
        }
    }
    if(info->Flags&0x4000) {
        if((t->Flags&16)==0) START_FUNC(info)(t);
        else {
            transform=zero_velocity;
            UpdateMiniModelStruct(t->owner,(f32 *)&transform,(f32 *)t->pos,t->mat.uvs[0]);
        }
    }
}
/* Gives target `t` (of kind `info`, record kind `category` = info->structureID) its record: its own of its kind's
 * array (gTargetOwnStructPtrs, kinds with flag 8; a path target's starts with its PTHD record, whose flag 0x10 makes
 * it not collide), else its kind's static one (gTargetStructInfoPtrs[category], record structureIdx; none for kind
 * 7). A box (collFuncIdx 4) takes the next bounding box model, at rest, with its row of BBoxCollInfo (collVal). In
 * a race under way (state bit 0x400000: battle drops mines then) a mine takes the first free mine record. The
 * records are the kinds' (heap blocks, or the game's data), in the N64's layout: the target keeps a pointer. */
void set_target_data(tTarget *t,TargInfo *info,s32 category)
{
    s32 index,j;
    tMiniModelData *matrix;
    tTargetStructMine *mine;
    if(!(gGameState&GS_RACING)) {
        if(info->Flags&8) {
            t->data=gTargetOwnStructPtrs[category];
            if(!(t->Flags&16) && category==4) {
                j=t->owner;
                STATE_PATH(t->data)=PC_N64W(&gTargetPaths.paths[j]);
                if(gTargetPaths.paths) {}
                if((PATH_OF(t->data))->flags&16) t->Flags&=~8;
            }
            gTargetOwnStructPtrs[category]=gTargetOwnStructPtrs[category]+gTargetStructSizes[category];
        } else if(category!=7) {
            t->data=gTargetStructInfoPtrs[category]+gTargetStructSizes[category]*info->structureIdx;
        }
        if(info->collFuncIdx==4) {
            t->owner=gBBoxModelUsed++;
            matrix=&gBBoxModelStructs[t->owner];
            CopyUV(unitmatrix,matrix->uvs[0]);
            for(index=0;index<3;index++) {
                matrix->vel[index]=0.0f;
                matrix->pos[index]=0.0f;
            }
            index=(s32)targetInfo[t->infoIdx].collVal;
            matrix->bboxRow=index;          /* MAT3: &BBoxCollInfo[index] */
        }
    } else if((info->Flags&8) && info->structureID==5) {
        mine=(tTargetStructMine *)gTargetStructPtrs[5];
        for(index=0;index<16;index++,mine++) {
            if(!mine->used && mine->dlIdx==-1 && mine->glowdls[0]==-1 && mine->glowdls[1]==-1) break;
        }
        mine->used=1;
        t->data=gTargetOwnStructPtrs[category]+gTargetStructSizes[category]*index;
    }
}

/* Makes a target of `obj` if its name starts with a kind's in targetInfo: a world object (`fromWorld`, under world
 * group `parent`, -2 the group at `pos`), a PTHD record (the target path `owner`), or a dropped mine (its name; at
 * `pos`, `owner` the car). Not made: in the demo without the Expansion Pak, a box in a ghost race when `col`, a kind
 * that needs the Expansion Pak without it, a kind with no flags but 4, one for the other direction (_FW, _BW). The
 * new node of gTargetList gets its model, kind, object definitions, place (`vis`: its scene object, unless the kind
 * is one player only in split screen), radius, collision flag and key; coins count gold and silver; boxes and
 * kinds with records are counted for SetTargetDefs. In a race under way it is defined at once (and in battle tested
 * against every car). Returns 1 if a kind matched (made or not), 0 if none did.
 * PC: video.detail high gives a kind that is one player only (0x100) its scene object in split screen too, as one
 * player has it (docs/port/DETAIL.md 2.5); PC_DETAIL_PLAYERS is pc_game_types.h's. */
s32 SetupTarget(tModelPart *obj, s16 parent, f32 *pos, s32 fromWorld, s8 owner, s32 vis, s32 col)
{
    s32 i;
    s32 pad152;
    s32 pad148;
    s32 superGroup;
    f32 *groupMat;
    tTarget *t;
    u32 dlflags;
    s32 pad128;
    s32 pad124;
    s8 targDir;
    TargInfo *info;
    u16 typeFlags;
    s16 objListIdx;
    s32 tail[8];

    if (fromWorld != 0) {
        dlflags = obj->dlflags;
    } else {
        dlflags = 0;
    }
    for (i = 0; i < 122; i++) {
        info = &targetInfo[i];
        if (game_strncmp(obj->name, info->name, strlen(info->name)) != 0) {
            continue;
        }
        if ((gGameState & GS_DEMO) && !expansion_pak_present) {
            return 1;
        }
        if (gRace == kGhost && info->collFuncIdx == 4 && col) {
            return 1;
        }
        if (!expansion_pak_present && !(info->Flags & 0x20)) {
            return 1;
        }
        if (!(info->Flags & ~4)) {
            return 1;
        }
        targDir = -1;
        if (game_strstr(obj->name, "_BW") != 0) {
            targDir = 1;
        }
        if (game_strstr(obj->name, "_FW") != 0) {
            targDir = 0;
        }
        if ((gBackwards && targDir == 0) || (!gBackwards && targDir == 1)) {
            return 1;
        }
        t = GetStatLLE(&gTargetList);
        if (t == 0) {
            return 1;
        }
        typeFlags = info->Flags;
        if (typeFlags & 8) {
            gTargetStructCount[info->structureID]++;
        }
        if (typeFlags & 0x8000) {
            gBBoxModelCount++;
        }
        t->mbIdx = MBOX_FindObject_WORK(info->mboxname, 0, (s8)(gNumModels[0] - 1), 1);
        t->infoIdx = i;
        objListIdx = info->objListIdx;
        t->slot = -1;
        t->type = objListIdx;
        t->otype = objListIdx;
        t->owner = owner;
        t->frames = 0;
        t->sound = -1;
        t->timeStamp = 0.0f;
        if (typeFlags & 0x400) {
            t->data = obj;   /* its world object (a flag's name: AnimateFlag) */
        }
        if (typeFlags & 0x80) {
            dlflags |= 0x8000;
        }
        if (info->Flags & 0x40) {
            dlflags |= 0x400000;
        }
        if (!(typeFlags & 4)) {
            if (info->structureID == 5) {
                t->Flags = 0;
            } else {
                t->Flags = 2;
            }
        } else {
            t->Flags = 0;
        }
        if (info->structureID == 4 && fromWorld != 0) {
            t->Flags |= 0x10;
        }
        if (info->structureID == 6) {
            if (game_strstr(obj->name, "GOLD") != 0) {
                t->Flags |= 0x40;
                gGoldCoinCnt++;
            } else {
                gSilverCoinCnt++;
            }
        }
        if (vis) {
            if (fromWorld != 0) {
                t->mat.pos[0] = obj->mat.pos[0];
                t->mat.pos[1] = obj->mat.pos[1];
                t->mat.pos[2] = obj->mat.pos[2];
                CopyUV(obj->mat.uvs[0], t->mat.uvs[0]);
            } else {
                t->mat.pos[0] = pos[0];
                t->mat.pos[1] = pos[1];
                t->mat.pos[2] = pos[2];
                CopyUV(unitmatrix, t->mat.uvs[0]);
            }
            if (typeFlags & 0x800) {
                dlflags |= 0x40000;
                superGroup = -1;
            } else if (parent == -2) {
                superGroup = FindWorldGroup(pos);
            } else {
                superGroup = parent;
            }
            if (info->structureID == 6) {
                superGroup = -1;
                CopyUV(unitmatrix, t->mat.uvs[0]);
                dlflags |= 0x52000;
            }
            if (PC_DETAIL_PLAYERS(gNumPlayers) < 2 || !(typeFlags & 0x100)) {
                t->dlIdx = ZOID_AddObject(t->mbIdx, t->mat.uvs[0], superGroup, dlflags);
            }
            if ((typeFlags & 0x300) && superGroup >= 0) {
                groupMat = (f32 *)ObjInstanceList[superGroup].mat;
                t->mat.pos[0] -= groupMat[9];
                t->mat.pos[1] -= groupMat[10];
                t->mat.pos[2] -= groupMat[11];
            }
            if (info->structureID == 4 && MBOX_GetObjectRadius(t->mbIdx) < 50.0f) {
                ObjInstanceList[t->dlIdx].flags = ZOID_ObjectFlags(t->dlIdx) | 0x20;
            }
            MBOX_GetPosition(t->pos, t->dlIdx);
        }
        if (info->collVal == -1.0f) {
            t->radius = MBOX_GetObjectRadius(t->mbIdx);
        } else {
            t->radius = info->collVal;
        }
        if (info->collFuncIdx != 6) {
            t->Flags |= 8;
        }
        if (fromWorld != 0) {
            t->mbIdx = obj->tidx;
        } else if (info->structureID == 4) {
            t->mbIdx = gTargetPaths.paths[owner].parent;
        }
        if (gGameState & GS_RACING) {
            if (gRace == kBattle) {
                ((tAlwaysCol *) GetStatLLE(&gAlwaysColList))->t = t;
            }
            SetTargetDef(t);
        }
        return 1;
    }
    return 0;
}

/* path i, as the N64 code reaches it: by its byte offset (offset = 36 * i) */
#define SCENE ((tBasicTargetPath *)((u8 *)gTargetPaths.paths + offset))

/* Sets up the target paths of this race's direction (a ghost race only those with a point flagged 0x1000000, the
 * others' collision objects removed): a target at each point flagged 1; a path for the other direction loses its
 * collision object if flagged 0x800. On a fresh load it allocates the kinds' records and the bounding box models
 * and finds, for each key of the target groups' list (gTargetGroupList), the colliding target it names (NULL for
 * none: MAT3 replaced the keys by the targets in place; the PC's list keeps both); in battle the mine records are
 * made free. Then defines every target (SetTargetDef) and, in battle, the power-ups. Not in the demo without the
 * Expansion Pak. */
void SetTargetDefs(void)
{
    s32 i, j, offset, enabled, other;
    tTarget *t;
    Vec3 zero;
    struct pc_target_group *group;
    tTargetStructMine *mine;

    if ((gGameState & GS_DEMO) && !expansion_pak_present) {
        return;
    }
    if (gTargetPaths.count) {
        zero.z = zero.x = zero.y = 0.0f;
        for (i = 0, offset = 0; i < gTargetPaths.count; i++, offset += 36) {
            if (gRace == kGhost && SCENE->collIdx > 0) {
                for (j = 0; j < SCENE->numPts; j++) {
                    if (PATH_PTS(SCENE)[j].flags & 0x01000000) {
                        goto process;
                    }
                }
                if (SCENE->collIdx > 0) {
                    RemoveCollisionObject(SCENE->collIdx);
                }
                continue;
            }
        process:
            if (((SCENE->flags & 4) && (SCENE->flags & 8)) || (gBackwards && (SCENE->flags & 8)) ||
                (!gBackwards && (SCENE->flags & 4))) {
                enabled = (SCENE->flags & 0x8000) ? 0 : 1;
                other = (SCENE->flags & 0x10) ? 0 : 1;
                for (j = 0; j < SCENE->numPts; j++) {
                    if (Restarting) {
                        PATH_PTS(SCENE)[j].flags &= ~0x1000;
                    }
                    if (PATH_PTS(SCENE)[j].flags & 1) {
                        SetupTarget((tModelPart *)SCENE, -1, &zero.x, 0, i, enabled, other);
                    }
                }
                if (Restarting && SCENE->collIdx > 0) {
                    ResetCollisionObject(SCENE->collIdx);
                }
            } else if (SCENE->flags & 0x800) {
                RemoveCollisionObject(SCENE->collIdx);
            }
        }
    }
    if (!Restarting) {
        for (i = 0; i < 8; i++) {
            if (gTargetStructCount[i]) {
                gTargetStructPtrs[i] = AllocateMemoryHeap(0, gTargetStructSizes[i] * gTargetStructCount[i]);
            }
        }
        if (gBBoxModelCount) {
            gBBoxModelStructs = AllocateMemoryHeap(0, gBBoxModelCount * 64);
        }
        for (i = 0; i < numgTargetGroupList; i++) {
            group = &gTargetGroupList[i];
            group->target = NULL;
            for (t = gTargetList.head; t != NULL; t = t->next) {
                if ((t->Flags & TARGET_COLLIDES) && t->mbIdx == group->key) {
                    group->target = t;
                    break;
                }
            }
        }
    }
    if (gRace == kBattle) {
        for (i = 0; i < 16; i++) {
            ((tTargetStructMine *)gTargetStructPtrs[5])[i].used = 0;
            ((tTargetStructMine *)gTargetStructPtrs[5])[i].dlIdx = -1;
            ((tTargetStructMine *)gTargetStructPtrs[5])[i].glowdls[0] = -1;
            ((tTargetStructMine *)gTargetStructPtrs[5])[i].glowdls[1] = -1;
        }
    }
    gBBoxModelUsed = 0;
    for (i = 0; i < 8; i++) {
        if (gTargetStructCount[i]) {
            gTargetOwnStructPtrs[i] = gTargetStructPtrs[i];
        }
    }
    for (t = gTargetList.head; t != NULL; t = t->next) {
        SetTargetDef(t);
    }
    if (gRace == kBattle) {
        InitBattlePowerUps(Restarting);
    }
}
