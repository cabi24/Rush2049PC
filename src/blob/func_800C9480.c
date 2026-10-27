/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef int s32;
typedef unsigned int u32;
typedef unsigned char u8;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
typedef struct Table {intptr_t value[3][5];} Table;   /* a name index or a profile handle (docs/port/CASTS.md) */
extern Table gTotalNames[12];
extern u8 D_80151968[260];
extern unsigned short D_80151A78[20];
extern void draw_minimap(void);
extern void *memset(void *,int,u32);
void func_800C9480(void) {
    s32 i,j,k;
    draw_minimap();
    for(i=0;i<12;i++)
        for(j=0;j<3;j++)
            for(k=0;k<5;k++) gTotalNames[i].value[j][k]=-1;
    memset(D_80151968,0,260);
    memset(D_80151A78,0,40);
}
