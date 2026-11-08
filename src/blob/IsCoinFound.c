/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned char u8;typedef signed char s8;typedef unsigned short u16;typedef unsigned int u32;typedef int s32;
typedef struct Data {u8 *base;} Data;
typedef struct Car {void *next; void *prev; void *fileh; u8 pad0_C[0x2C - 0xC];Data *data;} Car;
typedef struct Ref {Car *car;} Ref;
typedef struct Player76 {u8 pad0;u8 index;u8 pad2[70];Ref *ref;} Player76;
extern u32 gGameState;extern s8 gTrackNum;extern Player76 gPlayers[];extern Car *gJustPlayers[];
u32 IsCoinFound(s32 player,s32 bit) {
 s32 selector;Player76 *p;Ref *ref;Data *data;
 if(gGameState&8)return 1;
 selector=gTrackNum;
 if(selector>=0&&selector<6) {
  p=&gPlayers[player];ref=p->ref;
  if(!ref){return *(u16 *)(gJustPlayers[p->index]->data->base+232+selector*96)&(1u<<bit);}
  data=ref->car->data;if(!data)return 1;
  return *(u16 *)(data->base+232+selector*96)&(1u<<bit);
 }
 if(selector>=14&&selector<18) {
  p=&gPlayers[player];ref=p->ref;
  if(!ref){return *(u16 *)(gJustPlayers[p->index]->data->base+456+selector*64)&(1u<<bit);}
  return *(u16 *)(ref->car->data->base+456+selector*64)&(1u<<bit);
 }
 return 1;
}
