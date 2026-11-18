/* InitRecords: clear the high score tables (MAT3 gamerecords.c). */
#include "game/game.h"

extern void *memset(void *,int,u32);

/* Clears the totals over all profiles and the high score tables: every owner -1 (none), no names typed. */
void InitRecords(void) {
    s32 i,j,k;
    ClearTotalStats();
    for(i=0;i<12;i++)
        for(j=0;j<3;j++)
            for(k=0;k<5;k++) gTotalNames[i][j][k].name=-1;
    memset(gTempNames,0,260);
    memset(gTempNameAges,0,40);
}
