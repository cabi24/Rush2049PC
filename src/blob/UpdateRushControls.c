#include "game/game.h"

typedef struct Player76 {u8 index,selector;u8 gap[2];u32 previous,current,pressed;f32 horizontal,vertical;u8 opaque[0x48 - 0x18]; void *ref;} Player76;
typedef struct Axes8 {f32 horizontal,vertical;} Axes8;
extern Player76 gPlayers[];

VIEW(u32[], gContLevels);
#define gContLevels gContLevels_view
VIEW(u32[], gContEdges);
#define gContEdges gContEdges_view
VIEW(u32[], gAutoEdges);
#define gAutoEdges gAutoEdges_view
VIEW(Axes8[], gContStick);
#define gContStick gContStick_view

void UpdateRushControls(void) {
 int player;
 Player76 *record;
 UpdateControls();
 for(player=0;player<4;player++) {
  record=&gPlayers[player];
  if(record->selector==5 || player>=gNumPlayers) {
   record->current=0;
   record->previous=0;
   record->pressed=0;
   record->horizontal=0.0f;
   record->vertical=0.0f;
  } else {
   record->current=gContLevels[record->selector];
   record->previous=gContEdges[record->selector];
   record->pressed=gAutoEdges[record->selector];
   record->horizontal=gContStick[record->selector].horizontal;
   record->vertical=gContStick[record->selector].vertical;
  }
 }
}
