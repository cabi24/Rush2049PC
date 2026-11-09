#include "game/game.h"

typedef struct Record {uintptr_t word0,word4;u16 half8,halfA,halfC,padE,half10,half12;u8 tail[12];} Record;
VIEW(Record[], gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view
void ZOID_UpdateBlit(u32 index,u32 h8,uintptr_t w4,uintptr_t w0,u32 hA,u32 hC,u32 h10,u32 h12) {Record *r=&gBlitInstanceList[index];r->word4=w4;r->word0=w0;r->half8=h8;r->halfA=hA;r->halfC=hC;r->half10=h10;r->half12=h12;}
