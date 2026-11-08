/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef int s32;
typedef unsigned int u32;
typedef struct ResSlot {s8 f0,loaded,f2,f3,f4,f5;u8 type,pad07;s32 pad08;u8 **p0C;u8 *w10;} ResSlot;
typedef ResSlot Voice;
extern Voice gWaveInstance[64];
extern s32 LoadWave(s32 a, s32 b, s32 c, s32 d, void *e);
extern void LockWaveInstance(s32 a);

void LockAndLoad(s32 id);

extern unsigned int gGameState;
extern void UnloadWaveAll(s32);
extern void InitWaves(void);
extern void HAL_PurgeFonts(void);
void uninitGameState(void)
{
    unsigned int state=gGameState;
    if(state&0x4000) UnloadWaveAll(57);
    else if(state&0x100) {
        UnloadWaveAll(56);
        InitWaves();
    } else if(state&0x80) UnloadWaveAll(60);
    else if(state&0x20000000) {
        UnloadWaveAll(55);
        LockAndLoad(54);
        LockAndLoad(58);
        LockAndLoad(59);
    } else if(state&0x10000000) HAL_PurgeFonts();
}


typedef struct OSMesgQueue OSMesgQueue;
#define NULL ((void *)0)
extern s32 gWaveSizes[],bg_load_request;
extern u8 bg_load_idle_queue[],bg_load_request_queue[];
extern void MirrorWaveInstance(s32);
extern void *AllocateHandleHeap(void *,s32);
extern void *AllocateMemoryHeap(s32,s32);
extern s32 GetPtrSize(u32);
extern void SetWaveInstanceModel(s32);
extern void ReadWave(s32);
extern s32 osRecvMesg(OSMesgQueue *,void *,s32);
extern s32 osSendMesg(OSMesgQueue *,void *,s32);
s32 LoadWave(s32 kind, s32 skip, s32 async, s32 flag, void *buf);
