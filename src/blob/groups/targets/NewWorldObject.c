#include "game/game.h"

typedef struct Node104 {
    u8 name[16];
    f32 transform[12];
    u32 flags;
    s16 sibling,child;
    u8 opaque72[8];
    f32 minimum[3],maximum[3];
} Node104;
VIEW(Node104 *, WorldBase);
#define WorldBase WorldBase_view

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

extern int SetupTarget(Node104 *,s16,int,int,int,int,int);
extern int ZOID_AddObject ( int , f32 * , s16 , u32 );

void NewWorldObject(Node104 *node,s16 parent)
{
    Node104 *current;
    s16 first;
    int result;
    int model;
    first=-1;
    current=node;
    if(current) { if(cur_world_obj){} do {
        cur_world_obj=current;
        if(!SetupTarget(current,parent,0,1,-1,1,0)) {
            result=parent;
            model=MBOX_FindObject_WORK(current,0,gNumModels-1,1);
            if(gGameState & 8)
                result=(s16)ZOID_AddObject(model,current->transform,result,get_world_part_flags(current)|0xE00);
            else
                result=(s16)ZOID_AddObject(model,current->transform,result,current->flags);
            if(current->minimum[0]!=current->maximum[0] || current->minimum[1]!=current->maximum[1]) {
                if(parent==-1)ZOID_BuildCullingList(current->minimum,current->maximum,result);
            }
            if(Worlddlidx==-1)Worlddlidx=result;
            if(first==-1)first=result;
        }
        if(cur_world_obj){}
        if(current->sibling>=0)current=&WorldBase[current->sibling];
        else current=0;
    } while(current);
    }
    current=node;
    while(current) {
        if(current->child>=0)NewWorldObject(&WorldBase[current->child],first);
        first++;
        if(current->sibling>=0)current=&WorldBase[current->sibling];
        else current=0;
    }
    if(parent){}
    if(parent){}
    if(parent){}
    if(parent){}
}

u32 get_world_part_flags(Node104 *n)
{
    return n->flags;
}
