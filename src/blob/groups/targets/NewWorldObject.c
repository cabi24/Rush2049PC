/* targets/NewWorldObject.c: add a world tree's parts to the scene (MAT3 world.c, NewWorldObject(), static there).
 *
 * NewWorld calls it with a world's root part. Each part is a model part of the world file: a named model with its
 * place and draw flags, linked to its next sibling and its first child by index into WorldBase. A part that is a
 * target (SetupTarget: a moving or breakable track object) becomes that instead of a plain scene node. */
#include "game/game.h"

/* A world part (MAT3 tModelPart, 0x68 bytes). */
typedef struct tModelPart {
    u8 name[16];        /* +0x00 its model's name */
    f32 mat[12];        /* +0x10 its place: the orientation (3x3) and position (MAT3 tUVMatrix) */
    u32 dlflags;        /* +0x40 its scene node flags */
    s16 next,child;     /* +0x44 its next sibling and first child, indexes into WorldBase; -1 none */
    u8 opaque72[8];     /* +0x48 sfx1, sfx2, tidx */
    f32 min[3],max[3];  /* +0x50 its bounding box, for the culling list */
} tModelPart;
VIEW(tModelPart *, WorldBase);
#define WorldBase WorldBase_view

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

/* Add the part `wnode`, its siblings and (recursively) their children to the scene under the scene node `parent`
 * (-1: the top). Each part that is not a target becomes a scene node of its model (MBOX_FindObject_WORK by name)
 * with its flags; in the attract demo (GS_DEMO) with the flags get_world_part_flags gives and 0xE00 (hidden in
 * windows 1-3: the demo has one). A top-level part with a bounding box gets a culling list. The first node made is
 * kept in Worlddlidx (NewWorld's result). The children of the k-th part made at this level go under the scene
 * node first + k: the nodes of one level are consecutive. cur_world_obj is the part being added, for SetupTarget.
 * The empty ifs are the decompiled code's and keep the unoptimised code as it was. */
void NewWorldObject(tModelPart *wnode,s16 parent)
{
    tModelPart *ptr;
    s16 first;
    int dlindex;
    int mbobj;
    first=-1;
    ptr=wnode;
    if(ptr) { if(cur_world_obj){} do {
        cur_world_obj=ptr;
        if(!SetupTarget(ptr,parent,0,1,-1,1,0)) {
            dlindex=parent;
            mbobj=MBOX_FindObject_WORK(ptr,0,gNumModels-1,1);
            if(gGameState & 8)
                dlindex=(s16)ZOID_AddObject(mbobj,ptr->mat,dlindex,get_world_part_flags(ptr)|0xE00);
            else
                dlindex=(s16)ZOID_AddObject(mbobj,ptr->mat,dlindex,ptr->dlflags);
            if(ptr->min[0]!=ptr->max[0] || ptr->min[1]!=ptr->max[1]) {
                if(parent==-1)ZOID_BuildCullingList(ptr->min,ptr->max,dlindex);
            }
            if(Worlddlidx==-1)Worlddlidx=dlindex;
            if(first==-1)first=dlindex;
        }
        if(cur_world_obj){}
        if(ptr->next>=0)ptr=&WorldBase[ptr->next];
        else ptr=0;
    } while(ptr);
    }
    ptr=wnode;
    while(ptr) {
        if(ptr->child>=0)NewWorldObject(&WorldBase[ptr->child],first);
        first++;
        if(ptr->next>=0)ptr=&WorldBase[ptr->next];
        else ptr=0;
    }
    if(parent){}
    if(parent){}
    if(parent){}
    if(parent){}
}

/* The scene node flags of the world part n (its dlflags). */
u32 get_world_part_flags(tModelPart *n)
{
    return n->dlflags;
}
