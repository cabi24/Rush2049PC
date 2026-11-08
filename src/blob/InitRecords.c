/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef int s32;
typedef unsigned int u32;
typedef unsigned char u8;
typedef struct Table {intptr_t value[3][5];} Table;   /* a name index or a profile handle (docs/port/CASTS.md) */
extern Table gTotalNames[12];
extern u8 gTempNames[260];
extern unsigned short gTempNameAges[20];
extern void ClearTotalStats(void);
extern void *memset(void *,int,u32);
void InitRecords(void) {
    s32 i,j,k;
    ClearTotalStats();
    for(i=0;i<12;i++)
        for(j=0;j<3;j++)
            for(k=0;k<5;k++) gTotalNames[i].value[j][k]=-1;
    memset(gTempNames,0,260);
    memset(gTempNameAges,0,40);
}
