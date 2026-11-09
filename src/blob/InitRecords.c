/* InitRecords: clear the high score tables (MAT3 gamerecords.c). */
#include "game/game.h"

typedef struct Table {intptr_t value[3][5];} Table;   /* a name index or a profile handle (docs/port/CASTS.md) */
VIEW(Table[12], gTotalNames);
#define gTotalNames gTotalNames_view
VIEW(u8[260], gTempNames);
#define gTempNames gTempNames_view
VIEW(unsigned short[20], gTempNameAges);
#define gTempNameAges gTempNameAges_view

extern void *memset(void *,int,u32);

/* Clears the totals over all profiles and the high score tables: every owner -1 (none), no names typed. */
void InitRecords(void) {
    s32 i,j,k;
    ClearTotalStats();
    for(i=0;i<12;i++)
        for(j=0;j<3;j++)
            for(k=0;k<5;k++) gTotalNames[i].value[j][k]=-1;
    memset(gTempNames,0,260);
    memset(gTempNameAges,0,40);
}
