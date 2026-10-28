/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Group vsync, extended by: the command-0x0A writer
 * speed_set (internal; receives blend/amount in f20/f22 and the two byte
 * flags in s1/s2 through IPA, clobbers s0/s3) with all four of its in-unit
 * callers: vsync_wait (already locked), speed_mode0_wrapper,
 * speed_mode1_wrapper and continue_prompt.
 * Source of speed_set/vsync_wait is 's.
 * Shaping quirk: the three one-call wrappers must be written on several lines.
 * Written on one line each, as1 sinks the IPA saves of s3/s0 below the
 * argument moves (6/22, 6/22, 10/23 words off); the multi-line layout gives
 * retail's prologue order. continue_prompt's literal is written as 0.05f
 * (own .rodata 0x80124280 = 0x3D4CCCCD, verified by the scorer).
 */
typedef signed char s8;
typedef signed int s32;
typedef float f32;
typedef struct OSMesgQueue OSMesgQueue;
#define NULL ((void *)0)
#define M2C_FIELD(p,t,o) (*(t)((char *)(p)+(o)))
extern s32 D_80142728,D_801427A8;
extern void *func_80091B00(void);
extern s32 osRecvMesg(OSMesgQueue *,void *,s32);
extern s32 osSendMesg(OSMesgQueue *,void *,s32);
extern f32 D_801247EC;
extern s8 D_80146115,D_8010FFC0;
#ifdef TARGET_PC
/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's entity_transform_calc says when). A volume change (command 10) is then dropped: with no
 * message the lock is given back and nothing is sent, and a message the full queue refuses is given back (its
 * in-use byte, +3); the volume stays as it was until the next change. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){osSendMesg((OSMesgQueue *)&D_80142728,NULL,0);return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)M2C_FIELD(m,s8 *,3)=0
#else
#define PC_CMD_OR_UNLOCK(m)
#define PC_CMD_SEND(q,m) osSendMesg(q,m,0)
#endif
void speed_set(f32 blend,f32 amount,s32 first,s32 second) {
    f32 var_f0;
    void *temp_v0;

    osRecvMesg((OSMesgQueue *) &D_80142728, NULL, 1);
    temp_v0 = func_80091B00(); PC_CMD_OR_UNLOCK(temp_v0)
    M2C_FIELD(temp_v0, s8 *, 2) = 0xA;
    if (blend < 0.0f) {
        M2C_FIELD(temp_v0, f32 *, 4) = 0.0f;
    } else {
        if (blend > 1.0f) {
            var_f0 = 1.0f;
        } else {
            var_f0 = blend;
        }
        M2C_FIELD(temp_v0, f32 *, 4) = var_f0;
    }
    if (amount < 0.0f) {
        M2C_FIELD(temp_v0, f32 *, 8) = 0.0f;
    } else {
        M2C_FIELD(temp_v0, f32 *, 8) = (f32) amount;
    }
    M2C_FIELD(temp_v0, s8 *, 0xC) = (s8) first;
    M2C_FIELD(temp_v0, s8 *, 0xD) = (s8) second;
    osSendMesg((OSMesgQueue *) &D_80142728, NULL, 0);
    PC_CMD_SEND((OSMesgQueue *) &D_801427A8, temp_v0);
}

void speed_mode0_wrapper(f32 blend, f32 amount)
{
    speed_set(blend, amount, 1, 0);
}

void speed_mode1_wrapper(f32 blend, f32 amount)
{
    speed_set(blend, amount, 0, 1);
}

void continue_prompt(void)
{
    speed_set(0.0f, 0.05f, 1, 0);
}

void vsync_wait(s32 flag) {
    f32 blend;
    D_8010FFC0=flag;
    if(flag) blend=0.0f;
    else blend=(f32)D_80146115/10.0f;
    speed_set(blend,D_801247EC,0,1);
}
