#include "game/game.h"

typedef struct Resource {u8 *data;} Resource;
typedef struct Object {void *next; void *prev; void *fileh; u8 opaque_C[0x2C - 0xC];Resource *resource;} Object;
typedef struct Handle {Object *object;} Handle;
typedef struct Player {u8 index,selector;u8 opaque[70];Handle *handle;} Player;

extern Player gPlayers[];
VIEW(Object *[], gJustPlayers);
#define gJustPlayers gJustPlayers_view

#define FLAGS(base,stride,state,offset) (*(u16 *)((base)+(stride)*(state)+(offset)))
void FoundCoin(int player,int bit) {
 Player *record;
 u32 mask;
 if(gGameState&8)return;
 if(gTrackNum>=0 && gTrackNum<6) {
  record=&gPlayers[player];
  if(record->handle==0) {
   mask=1u<<bit;
   FLAGS(gJustPlayers[record->selector]->resource->data,96,gTrackNum,232)|=mask;
   FLAGS(gJustPlayers[record->selector]->resource->data,96,gTrackNum,808)|=mask;
  } else if(record->handle->object->resource!=0) {
   mask=1u<<bit;
   FLAGS(record->handle->object->resource->data,96,gTrackNum,232)|=mask;
   FLAGS(record->handle->object->resource->data,96,gTrackNum,808)|=mask;
   SavePlayerCoins(record->handle,(u8)gTrackNum);
  }
 } else if(gTrackNum>=14 && gTrackNum<18) {
  record=&gPlayers[player];
  if(record->handle==0) {
   FLAGS(gJustPlayers[record->selector]->resource->data,64,gTrackNum,456)|=1u<<bit;
  } else {
   FLAGS(record->handle->object->resource->data,64,gTrackNum,456)|=1u<<bit;
   SavePlayerCoins(record->handle,(u8)gTrackNum);
  }
 }
}
