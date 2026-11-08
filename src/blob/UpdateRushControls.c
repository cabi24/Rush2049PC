/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef unsigned char u8;typedef signed short s16;typedef unsigned int u32;typedef float f32;
typedef struct Player76 {u8 index,selector;u8 gap[2];u32 previous,current,pressed;f32 horizontal,vertical;u8 opaque[0x48 - 0x18]; void *ref;} Player76;
typedef struct Axes8 {f32 horizontal,vertical;} Axes8;
extern Player76 gPlayers[];
extern s16 gNumPlayers;
extern u32 gContLevels[],gContEdges[],gAutoEdges[];
extern Axes8 gContStick[];
extern void UpdateControls(void);
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
