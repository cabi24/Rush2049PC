#include "game/game.h"

typedef struct Vec3 { f32 x, y, z; } Vec3;

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Ent {
    s32 flags;
    s32 w4;
    f32 *owner;
    f32 scaleA;
    f32 scaleB;
    u16 id;
    s16 link[3];
    void *w28[8];
    s32 w60;
    s32 w64;
} Ent;

typedef struct Resource68 { u8 prefix[64]; u32 flags; } Resource68;

/* PC: Scene36 is a track WAD's PTHD record, which keeps its N64 layout (port/assets/wad.c): resources (pts) is a
 * 32-bit window address of the path points. */
#define PTHD_PTS_FIELD u32 resources
#define PTHD_PTS(scene) ((Resource68 *)PC_N64P((scene)->resources))
typedef struct Scene36 {
    char name[16];
    u32 flags;
    s16 count;
    s16 action;
    u32 field24;
    PTHD_PTS_FIELD;
    s32 key;
} Scene36;

/* PC: a level-object node (a target, 112 bytes) keeps its N64 layout: the nodes are the arena block gTargetArray, and
 * next and state hold 32-bit window addresses (state: a heap record of the node's category, a track WAD record or
 * a static table). So does the dead-reckoning record Matrix64 (a heap block), whose record is arena data
 * (docs/port/POINTERS.md 10). */
enum n64p_Node { n64p_Node_ };
enum n64p_void { n64p_void_ };
enum n64p_u8 { n64p_u8_ };
typedef struct Node {
    enum n64p_Node next;
    u8 flags;
    s32 key;
    s32 handle;
    s16 metadata;
    f32 matrix[9];
    f32 pos[3];
    u8 xform[12];
    s16 f80;
    f32 f84;
    s16 texture;
    s16 f90;
    s8 f92;
    s32 f96;
    s8 f100;
    s8 index;
    f32 f104;
    enum n64p_void state;
} Node;

/* PC: every field spelled out, whichever view the loader converts the ROM's records by (docs/port/POINTERS.md 10). */
typedef struct Metadata48 {
    char *name;
    char *file;
    void (*callback)(Node *);
    void *animation;
    s16 kind;
    u16 flags;
    s16 f20;
    s8 category;
    s8 element;
    f32 value;
    s32 sound, w20, w24, w28;
    f32 f2C;
} Metadata48;

typedef struct Place { char name[16]; f32 matrix[9]; f32 pos[3]; u32 flags; s32 w68; s32 w72; s32 key; } Place;
typedef struct Slot136 { s32 object[3]; u8 gap12[122]; s16 active; } Slot136;
typedef struct Matrix64 { enum n64p_u8 record; f32 matrix[9]; f32 position[3]; f32 velocity[3]; } Matrix64;
#define N64P(x) _Generic((x), enum n64p_Node: (Node *)PC_N64P(x), enum n64p_void: (void *)PC_N64P(x), \
    enum n64p_u8: (u8 *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
/* A camera node's state (CamCtl, groups/targetpaths) starts with its scene (a PTHD record) as a u32. */
#define STATE_SCENE(s) (*(u32 *)N64P(s))
#define SCENE_OF(s) ((Scene36 *)PC_N64P(*(u32 *)N64P(s)))
/* GTLD keys: the side array holds them sign-extended (docs/port/LOADERS.md), and so does this. */
#define KEY(k) (void *)(intptr_t)k
/* An info record with flag 0x400 keeps the place record as the node's state, a 32-bit window address. A place record
 * is the track WAD's (the heap, in the window), except for the mine shots.c drops, which passes its name, a host
 * string: that one is not kept, the state stays 0 (docs/port/LAYOUT.md 6). */
#define PLACE_STATE(x) ((uintptr_t)(x) >> 16 >> 16 ? 0u : N64W(x))
typedef struct Animation24 {
    struct Animation24 *next;
    s16 state;
    u16 field6;
    u32 field8;
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
    u32 node;
    f32 time;
    void *data;
} Animation24;
typedef struct Pool { u8 b0; s32 count; s32 size; u8 *mem; Node *head; u8 *free; } Pool;

/* The level's path scenes (waves.c's anon_paths_count_8, MAT3 gTargetPaths): count of them at paths. */
typedef struct anon_paths_count_8 { Scene36 *paths; s32 count; } anon_paths_count_8;
VIEW(anon_paths_count_8, gTargetPaths);
#define gTargetPaths gTargetPaths_view

VIEW(s8[8], gTargetStructCount);
#define gTargetStructCount gTargetStructCount_view

VIEW(u8 *[8], gTargetStructPtrs);
#define gTargetStructPtrs gTargetStructPtrs_view
VIEW(u8 *[8], gTargetOwnStructPtrs);
#define gTargetOwnStructPtrs gTargetOwnStructPtrs_view

VIEW(Matrix64 *, gBBoxModelStructs);
#define gBBoxModelStructs gBBoxModelStructs_view

VIEW(void **, gTargetGroupList);
#define gTargetGroupList gTargetGroupList_view
VIEW(Pool, gTargetList);
#define gTargetList gTargetList_view
VIEW(Pool, gAlwaysColList);
#define gAlwaysColList gAlwaysColList_view
VIEW(Metadata48[122], targetInfo);
#define targetInfo targetInfo_view

VIEW(Ent[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view
VIEW(Animation24 *, gEnvList);
#define gEnvList gEnvList_view
VIEW(f32[], unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(f32[115], BBoxCollInfo);
#define BBoxCollInfo BBoxCollInfo_view
#define BODY_TABLE ((u8 *)BBoxCollInfo)
VIEW(Vec3, zero_velocity);
#define zero_velocity zero_velocity_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

extern Animation24 *GrabEnvEntry(void);

extern int strlen(const char *);

extern s32 ZOID_AddObject(s32, void *, s16, s32);

void SetTargetDef(Node *node)
{
    Metadata48 *metadata;
    s32 category;
    s32 resource;
    u16 value;
    Animation24 *animation;
    Vec3 transform;
    metadata=&targetInfo[node->metadata];
    category=metadata->category;
    set_target_data(node,metadata,category);
    if(node->texture!=-1) {
        resource=(s16)node->handle;
        value=gObjList[node->texture];
        ObjInstanceList[resource].id=value;
    }
    if(metadata->flags&2) {
        animation=GrabEnvEntry();
        if(animation) {
            animation->state=0;
            animation->data=metadata->animation;
            animation->node=N64W(node);
            animation->time=0.0f;
            animation->next=gEnvList;
            gEnvList=animation;
            if((metadata->flags&4) && metadata->callback) metadata->callback(node);
        }
    }
    if(metadata->flags&0x4000) {
        if((node->flags&16)==0) metadata->callback(node);
        else {
            transform=zero_velocity;
            UpdateMiniModelStruct(node->index,(f32 *)&transform,(f32 *)node->xform,node->matrix);
        }
    }
}
void set_target_data(Node *node,Metadata48 *metadata,s32 category)
{
    s32 index,j;
    Matrix64 *matrix;
    Scene36 *scene;
    Slot136 *slot;
    if(!(gGameState&0x400000)) {
        if(metadata->flags&8) {
            node->state=N64W(gTargetOwnStructPtrs[category]);
            if(!(node->flags&16) && category==4) {
                j=node->index;
                STATE_SCENE(node->state)=N64W(&gTargetPaths.paths[j]);
                if(gTargetPaths.paths) {}
                if((SCENE_OF(node->state))->flags&16) node->flags&=~8;
            }
            gTargetOwnStructPtrs[category]=gTargetOwnStructPtrs[category]+gTargetStructSizes[category];
        } else if(category!=7) {
            node->state=N64W(gTargetStructInfoPtrs[category]+gTargetStructSizes[category]*metadata->element);
        }
        if(metadata->kind==4) {
            node->index=gBBoxModelUsed++;
            matrix=&gBBoxModelStructs[node->index];
            CopyUV(unitmatrix,matrix->matrix);
            for(index=0;index<3;index++) {
                matrix->velocity[index]=0.0f;
                matrix->position[index]=0.0f;
            }
            index=(s32)targetInfo[node->metadata].value;
            matrix->record=N64W(BODY_TABLE+index*32);
        }
    } else if((metadata->flags&8) && metadata->category==5) {
        slot=(Slot136 *)gTargetStructPtrs[5];
        for(index=0;index<16;index++,slot++) {
            if(!slot->active && slot->object[0]==-1 && slot->object[1]==-1 && slot->object[2]==-1) break;
        }
        slot->active=1;
        node->state=N64W(gTargetOwnStructPtrs[category]+gTargetStructSizes[category]*index);
    }
}

/* PC: video.detail high gives a level object whose metadata says one player only (0x100) its scene node in split
 * screen too, as one player has it (docs/port/DETAIL.md 2.5); PC_DETAIL_PLAYERS is pc_game_types.h's. */
s32 SetupTarget(Place *rec, s16 parent, f32 *pos, void *src, s8 index, s32 place, s32 other)
{
    s32 i;
    s32 pad152;
    s32 pad148;
    s32 res;
    f32 *owner;
    Node *node;
    u32 flags;
    s32 pad128;
    s32 pad124;
    s8 team;
    Metadata48 *md;
    u16 mflags;
    s16 shape;
    s32 tail[8];

    if (src != 0) {
        flags = rec->flags;
    } else {
        flags = 0;
    }
    for (i = 0; i < 122; i++) {
        md = &targetInfo[i];
        if (game_strncmp(rec->name, md->name, strlen(md->name)) != 0) {
            continue;
        }
        if ((gGameState & 8) && !expansion_pak_present) {
            return 1;
        }
        if (gRace == 2 && md->kind == 4 && other) {
            return 1;
        }
        if (!expansion_pak_present && !(md->flags & 0x20)) {
            return 1;
        }
        if (!(md->flags & ~4)) {
            return 1;
        }
        team = -1;
        if (game_strstr(rec->name, "_BW") != 0) {
            team = 1;
        }
        if (game_strstr(rec->name, "_FW") != 0) {
            team = 0;
        }
        if ((gBackwards && team == 0) || (!gBackwards && team == 1)) {
            return 1;
        }
        node = GetStatLLE(&gTargetList);
        if (node == 0) {
            return 1;
        }
        mflags = md->flags;
        if (mflags & 8) {
            gTargetStructCount[md->category]++;
        }
        if (mflags & 0x8000) {
            gBBoxModelCount++;
        }
        node->key = MBOX_FindObject_WORK(md->file, 0, (s8)(gNumModels - 1), 1);
        node->metadata = i;
        shape = md->f20;
        node->f92 = -1;
        node->f80 = shape;
        node->texture = shape;
        node->index = index;
        node->f90 = 0;
        node->f96 = -1;
        node->f104 = 0.0f;
        if (mflags & 0x400) {
            node->state = PLACE_STATE(rec);
        }
        if (mflags & 0x80) {
            flags |= 0x8000;
        }
        if (md->flags & 0x40) {
            flags |= 0x400000;
        }
        if (!(mflags & 4)) {
            if (md->category == 5) {
                node->flags = 0;
            } else {
                node->flags = 2;
            }
        } else {
            node->flags = 0;
        }
        if (md->category == 4 && src != 0) {
            node->flags |= 0x10;
        }
        if (md->category == 6) {
            if (game_strstr(rec->name, "GOLD") != 0) {
                node->flags |= 0x40;
                gGoldCoinCnt++;
            } else {
                gSilverCoinCnt++;
            }
        }
        if (place) {
            if (src != 0) {
                node->pos[0] = rec->pos[0];
                node->pos[1] = rec->pos[1];
                node->pos[2] = rec->pos[2];
                CopyUV(rec->matrix, node->matrix);
            } else {
                node->pos[0] = pos[0];
                node->pos[1] = pos[1];
                node->pos[2] = pos[2];
                CopyUV(unitmatrix, node->matrix);
            }
            if (mflags & 0x800) {
                flags |= 0x40000;
                res = -1;
            } else if (parent == -2) {
                res = FindWorldGroup(pos);
            } else {
                res = parent;
            }
            if (md->category == 6) {
                res = -1;
                CopyUV(unitmatrix, node->matrix);
                flags |= 0x52000;
            }
            if (PC_DETAIL_PLAYERS(gNumPlayers) < 2 || !(mflags & 0x100)) {
                node->handle = ZOID_AddObject(node->key, node->matrix, res, flags);
            }
            if ((mflags & 0x300) && res >= 0) {
                owner = ObjInstanceList[res].owner;
                node->pos[0] -= owner[9];
                node->pos[1] -= owner[10];
                node->pos[2] -= owner[11];
            }
            if (md->category == 4 && MBOX_GetObjectRadius(node->key) < 50.0f) {
                ObjInstanceList[node->handle].flags = ZOID_ObjectFlags(node->handle) | 0x20;
            }
            MBOX_GetPosition(node->xform, node->handle);
        }
        if (md->value == -1.0f) {
            node->f84 = MBOX_GetObjectRadius(node->key);
        } else {
            node->f84 = md->value;
        }
        if (md->kind != 6) {
            node->flags |= 8;
        }
        if (src != 0) {
            node->key = rec->key;
        } else if (md->category == 4) {
            node->key = gTargetPaths.paths[index].key;
        }
        if (gGameState & 0x400000) {
            if (gRace == 6) {
                ((Node **)GetStatLLE(&gAlwaysColList))[1] = node;
            }
            SetTargetDef(node);
        }
        return 1;
    }
    return 0;
}

#define SCENE ((Scene36 *)((u8 *)gTargetPaths.paths + offset))

void SetTargetDefs(void)
{
    s32 i, j, offset, enabled, other;
    Node *node;
    Vec3 zero;
    void *key;
    Slot136 *slot;

    if ((gGameState & 8) && !expansion_pak_present) {
        return;
    }
    if (gTargetPaths.count) {
        zero.z = zero.x = zero.y = 0.0f;
        for (i = 0, offset = 0; i < gTargetPaths.count; i++, offset += 36) {
            if (gRace == 2 && SCENE->action > 0) {
                for (j = 0; j < SCENE->count; j++) {
                    if (PTHD_PTS(SCENE)[j].flags & 0x01000000) {
                        goto process;
                    }
                }
                if (SCENE->action > 0) {
                    RemoveCollisionObject(SCENE->action);
                }
                continue;
            }
        process:
            if (((SCENE->flags & 4) && (SCENE->flags & 8)) || (gBackwards && (SCENE->flags & 8)) ||
                (!gBackwards && (SCENE->flags & 4))) {
                enabled = (SCENE->flags & 0x8000) ? 0 : 1;
                other = (SCENE->flags & 0x10) ? 0 : 1;
                for (j = 0; j < SCENE->count; j++) {
                    if (Restarting) {
                        PTHD_PTS(SCENE)[j].flags &= ~0x1000;
                    }
                    if (PTHD_PTS(SCENE)[j].flags & 1) {
                        SetupTarget((Place *)SCENE, -1, &zero.x, 0, i, enabled, other);
                    }
                }
                if (Restarting && SCENE->action > 0) {
                    ResetCollisionObject(SCENE->action);
                }
            } else if (SCENE->flags & 0x800) {
                RemoveCollisionObject(SCENE->action);
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
            key = gTargetGroupList[i];
            node = gTargetList.head;
            while (node != 0) {
                if ((node->flags & 8) && KEY(node->key) == key) {
                    gTargetGroupList[i] = node;
                    break;
                }
                node = N64P(node->next);
            }
            if (key == gTargetGroupList[i]) {
                gTargetGroupList[i] = 0;
            }
        }
    }
    if (gRace == 6) {
        for (i = 0; i < 16; i++) {
            ((Slot136 *)gTargetStructPtrs[5])[i].active = 0;
            ((Slot136 *)gTargetStructPtrs[5])[i].object[0] = -1;
            ((Slot136 *)gTargetStructPtrs[5])[i].object[1] = -1;
            ((Slot136 *)gTargetStructPtrs[5])[i].object[2] = -1;
        }
    }
    gBBoxModelUsed = 0;
    for (i = 0; i < 8; i++) {
        if (gTargetStructCount[i]) {
            gTargetOwnStructPtrs[i] = gTargetStructPtrs[i];
        }
    }
    node = gTargetList.head;
    while (node != 0) {
        SetTargetDef(node);
        node = N64P(node->next);
    }
    if (gRace == 6) {
        InitBattlePowerUps(Restarting);
    }
}

