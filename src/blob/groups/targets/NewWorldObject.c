/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * NewWorldObject (historical label; 0x800AC3D8, 648 bytes) and its node-flags getter
 * get_world_part_flags (0x800AC660, the caller-less retail stub right after it).
 * Recursive placement of a 104-byte node tree (pool WorldBase, sibling s16 @68, child s16 @70):
 * phase 1 walks the sibling chain, publishes the current node in cur_world_obj, instantiates it through
 * SetupTarget(node, parent, 0, 1, -1, 1, 0), and when that returns 0 resolves the name id
 * (MBOX_FindObject_WORK) and registers it with ZOID_AddObject (flags | 0xE00 when gGameState bit 3),
 * records bounds when parent == -1, and remembers the first id; phase 2 recurses into each child with
 * consecutive ids starting at that first id. No arcade ancestor identified.
 *
 * EQUAL in the whole-program unit, with level_objects' SetupTarget using the
 * kept entity getter ZOID_ObjectFlags instead of get_world_part_flags (see group.json).
 *
 * Recovered: s16 parameter (entry cvt and a1 home store); int result with (s16) casts on the
 * ZOID_AddObject returns (keeps the per-iteration result = parent copy in s1); volatile gNumModels;
 * the inlined getter get_world_part_flags in the 0xE00 branch only (retail's lw v0 / ori a3,v0 delay slot).
 *
 * Shaping devices (all code-free; disclosed per the owner rules; they may not be the original
 * spelling, probably compiled-out debug checks):
 *  - four compiled-out reads `if(parent){}` at the end: they lower parent's colouring priority
 *    (54/13 = 4.15) below the constant -1's (30/7 = 4.29), so -1 takes s3 and parent s4 as in retail;
 *  - `if(cur_world_obj){}` inside the first loop: keeps the &cur_world_obj address web above the
 *    per-procedure callee-saved cost the reads above raise (tot 20, else it splits and the frame is 72);
 *  - `if(current) { if(cur_world_obj){} do {...} while(current); }` (a while loop written as guarded
 *    do-while with a compiled-out read in the preheader): adds one block to the &cur_world_obj web
 *    (20/8 = 2.5), so it ranks below &Worlddlidx (20/7) and takes s8 (retail) instead of s7. The read
 *    must be on the same source line as the `do {` (a separate line changes as1's preheader schedule).
 */
typedef unsigned char u8;typedef signed char s8;typedef signed short s16;typedef unsigned int u32;typedef float f32;
typedef struct Node104 {
    u8 name[16];
    f32 transform[12];
    u32 flags;
    s16 sibling,child;
    u8 opaque72[8];
    f32 minimum[3],maximum[3];
} Node104;
extern Node104 *WorldBase,*cur_world_obj;
extern s16 Worlddlidx;
extern volatile u8 gNumModels;
extern u32 gGameState;
extern int SetupTarget(Node104 *,s16,int,int,int,int,int);
extern int MBOX_FindObject_WORK(void *,s8,s8,int),ZOID_AddObject(int,f32 *,s16,u32);
extern void ZOID_BuildCullingList(f32 *,f32 *,s16);
u32 get_world_part_flags(Node104 *n);
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
