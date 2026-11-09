#include "game/game.h"

typedef struct Handle Handle;
/* PC: a profile's pak file handle is at +16 (primary_id), a ghost's at +8 (secondary_id): their host layouts
 * (pc_prelude.h). The handle is a host pointer: PS_DeleteFile passes it whole. */
typedef struct Object {Handle *next;uintptr_t secondary_id,primary_id;} Object;
struct Handle {Object *object;};
typedef struct List16 {u8 indirect,doubly;u8 gap[2];int count;Handle *head,*tail;} List16;
typedef struct Player76 {u8 opaque[72];Handle *handle;} Player76;
VIEW(List16, gPlayerFiles);
#define gPlayerFiles gPlayerFiles_view
VIEW(List16, gGhostFiles);
#define gGhostFiles gGhostFiles_view
extern Player76 gPlayers[];

void RemovePlayerFile(uintptr_t identifier) {
 Handle *node,*pending;
 int i;
 for(node=gPlayerFiles.head;node!=0;node=node->object->next) {
  if(node->object->primary_id==identifier) {
   for(i=0;i<gNumPlayers;i++) {
    if(gPlayers[i].handle==node)gPlayers[i].handle=0;
   }
   RemovePlayerScores(node);
   RemoveLLE(&gPlayerFiles,node);
   return;
  }
 }
 for(pending=gGhostFiles.head;pending!=0;pending=pending->object->next) {
  if(pending->object->secondary_id==identifier) {
   RemoveLLE(&gGhostFiles,pending);
   pending->object=0;
   return;
  }
 }
}
