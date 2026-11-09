#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */
#define OSThread_s OSThread_sView   /* this file's own OSThread_s: include/game's OSThread_s is another type */
#define __OSThreadContext __OSThreadContextView   /* this file's own __OSThreadContext: include/game's __OSThreadContext is another type */

typedef struct __OSThreadContext
{
  u64 at;
  u64 v0;
  u64 v1;
  u64 a0;
  u64 a1;
  u64 a2;
  u64 a3;
  u64 t0;
  u64 t1;
  u64 t2;
  u64 t3;
  u64 t4;
  u64 t5;
  u64 t6;
  u64 t7;
  u64 s0;
  u64 s1;
  u64 s2;
  u64 s3;
  u64 s4;
  u64 s5;
  u64 s6;
  u64 s7;
  u64 t8;
  u64 t9;
  u64 gp;
  u64 sp;
  u64 s8;
  u64 ra;
  u64 lo;
  u64 hi;
  u32 sr;
  u32 pc;
  u32 cause;
  u32 badvaddr;
  u32 rcp;
  u32 fpcsr;
  f32 fp0;
  f32 fp2;
  f32 fp4;
  f32 fp6;
  f32 fp8;
  f32 fp10;
  f32 fp12;
  f32 fp14;
  f32 fp16;
  f32 fp18;
  f32 fp20;
  f32 fp22;
  f32 fp24;
  f32 fp26;
  f32 fp28;
  f32 fp30;
} __OSThreadContext;
typedef struct OSThread_s
{
  struct OSThread_s *next;
  s32 priority;
  struct OSThread_s **queue;
  struct OSThread_s *tlnext;
  u16 state;
  u16 flags;
  s32 id;
  s32 fp;
  struct __OSThreadprofile_s *thprof;
  __OSThreadContext context;
} OSThread;

typedef struct OSPfs
{
  s32 status;
  OSMesgQueue *queue;
  s32 channel;
  u8 id[32];
  u8 label[32];
  s32 version;
  s32 dir_size;
  s32 inode_table;
  s32 minode_table;
  s32 dir_table;
  s32 inode_start_page;
  u8 banks;
  u8 activebank;
} OSPfs;

s32 osGetThreadId(OSPfs *pfs, s32 pages);

void update_frame_on_game_thread(s32 arg0, s32 arg1)
{
  if (arg0) {}
  if (osGetThreadId(0, arg1) == 7)
  {
    UpdateFrame();
  }
}

