/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef int s32;
typedef unsigned int u32;
typedef struct ResSlot {s8 f0,loaded,f2,f3,f4,f5;u8 type,pad07;s32 pad08;u8 **p0C;u8 *w10;} ResSlot;
typedef ResSlot Voice;
extern Voice gWaveInstance[64];
extern s32 audio_frame_sync(s32 a, s32 b, s32 c, s32 d, void *e);
extern void LockWaveInstance(s32 a);

void resource_slot_clear(s32 id);

extern unsigned int D_801174B4;
extern void wheel_setup_initial(s32);
extern void InitWaves(void);
extern void player_cleanup_slots(void);
void state_change_preprocess(void)
{
    unsigned int state=D_801174B4;
    if(state&0x4000) wheel_setup_initial(57);
    else if(state&0x100) {
        wheel_setup_initial(56);
        InitWaves();
    } else if(state&0x80) wheel_setup_initial(60);
    else if(state&0x20000000) {
        wheel_setup_initial(55);
        resource_slot_clear(54);
        resource_slot_clear(58);
        resource_slot_clear(59);
    } else if(state&0x10000000) player_cleanup_slots();
}


typedef struct OSMesgQueue OSMesgQueue;
#define NULL ((void *)0)
extern s32 D_8002E580[],D_801569AC;
extern u8 D_80035440[],D_80156BB0[];
extern void entity_render_mode(s32);
extern void *AllocateHandleHeap(void *,s32);
extern void *AllocateMemoryHeap(s32,s32);
extern s32 audio_buffer_sync(u32);
extern void func_800972C4(s32);
extern void func_80097164(s32);
extern s32 osRecvMesg(OSMesgQueue *,void *,s32);
extern s32 osSendMesg(OSMesgQueue *,void *,s32);
s32 audio_frame_sync(s32 kind, s32 skip, s32 async, s32 flag, void *buf);
