#include "game/game.h"

typedef struct Record {u16 id,slot;u8 pad4[28];} Record;
typedef struct {u16 mode;u8 pad2[22];} Dest;

VIEW(Record *, stree_save);
#define stree_save stree_save_view
VIEW(Dest *, stree_surf);
#define stree_surf stree_surf_view
void RemoveCollisionObject(u32 id) {s32 i;Record *p=stree_save;for(i=0;i<stree_savenum;i++,p++){if(p->id==id)stree_surf[p->slot].mode=15;}}
