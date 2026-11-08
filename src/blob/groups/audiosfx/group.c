/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * SendCmd unit (sound command queue; N64-only, no arcade ancestor found).
 * Free list gFreeQ, active list gCmdQ, 24-byte sound entries gSfxInstances indexed by handle & 255.
 * SendCmd is internal (IPA: node in s4, s0-s3 left to the callers); FindChangeCmd (locked single,
 * byte-identical copy in FindChangeCmd.c) is internal too (its callers keep a2 live across the call).
 * Deleted inlined helpers, non-static and named after their retail stubs so each stub lands in its own slot
 * : sfx_cmd_list_add = list_append, GetSfxInstance = entry_alloc (stub identity among the three stubs
 * SendCmd is a HYPOTHESIS: the stubs are identical `jr ra; nop`), NewCmd = node_alloc (the stub
 * right before AudioSetMono, its first caller). FreeSfxInstance stays the locked empty stub (unclaimed).
 * DISCLOSED SHAPING DEVICES:
 * 1. SendCmd: compiled-out `if (node->kind == 2) {}` first in the then-arm: gives the constant
 *     2 a second, codeless use so it is a coloured web (retail `li a0,2`).
 * 2. node_alloc (NewCmd): compiled-out `do {} while (0);` after the head load, e.g. a compiled-out
 *     ASSERT(node). It ends the basic block before the unlink call; the &gFreeQ web then spans two blocks and
 *     a0 (precoloured for the call's argument in the second block) is forbidden to it, so it takes a2 as in retail
 *     AudioSetMono (`lui a2; addiu a0,a2,...`). Inert for the other five node_alloc callers.
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 */
typedef signed char s8;typedef unsigned char u8;typedef signed int s32;typedef unsigned int u32;typedef float f32;
typedef struct Entry {s8 flags[6];u8 pad6[2];s32 handle;s32 resource;u32 state16;s32 state20;} Entry;
typedef struct Node {struct Node *prev,*next;u8 kind,busy;u8 pad10[2];Entry *entry;f32 value,stream,priority,route;} Node;
typedef struct List {u32 pad0[2];Node *tail,*head;} List;
extern List gFreeQ,gCmdQ;
extern Entry *gSfxInstances;
extern u32 gNumInstances,gNextIndex;
void RemoveLLE(List *,Node *);
void InsertLLE(List *,Node *,Node *);
Node *FindChangeCmd(Entry *key);
/* PC: an effect start that finds no free entry (AudioSfxInit's, gNumInstances of them) is counted (--log-voices,
 * port/config/voices.c, docs/port/AUDIO.md 10). */
#define PC_COUNTED(what, value) (pc_snd_count(what), value)
void sfx_cmd_list_add(List *list,Node *node) {
 InsertLLE(list,node,list->tail);
}
s32 GetSfxInstance(Entry **out)
{
 u32 i;
 *out=0;
 for(i=0;i<gNumInstances;i++) {
  if(gSfxInstances[i].flags[0]==0) {
   gSfxInstances[i].state16=-1;
   gSfxInstances[i].flags[0]=1;
   gSfxInstances[i].flags[1]=0;
   gSfxInstances[i].flags[2]=0;
   gSfxInstances[i].flags[3]=0;
   gSfxInstances[i].flags[4]=0;
   gSfxInstances[i].flags[5]=0;
   gSfxInstances[i].handle=(gNextIndex<<8)|i;
   if(gNextIndex>=0xFFFFFF)gNextIndex=1;
   else gNextIndex++;
   *out=&gSfxInstances[i];
   return 1;
  }
 }
 return 0;
}
void SendCmd(Node *node) {
 Entry *entry;Node *cur,*next;
 if(node->kind!=4 && node->kind==2 && (entry=node->entry)->flags[1]==0) {
  if(node->kind==2){}
  cur=gCmdQ.head;
  while(cur) {
   next=cur->next;
   if(entry==cur->entry) {
    RemoveLLE(&gCmdQ,cur);
    sfx_cmd_list_add(&gFreeQ,cur);
    entry=node->entry;
   }
   cur=next;
  }
  entry->flags[0]=0;entry->handle=0;
  sfx_cmd_list_add(&gFreeQ,node);
 } else {
  sfx_cmd_list_add(&gCmdQ,node);
 }
}
Node *NewCmd(void) {
 Node *node=gFreeQ.head;
 do{}while(0);
 node->busy=0;
 RemoveLLE(&gFreeQ,node);
 return node;
}
void AudioSetMono(s32 value) {
 Node *node=NewCmd();
 node->kind=4;node->value=(f32)value;node->entry=0;
 SendCmd(node);
}
void AudioSetEffectPitch(s32 handle,f32 value) {
    Node *node;
    if(handle!=-1) {
        if(handle==gSfxInstances[handle&255].handle) {
            node=FindChangeCmd(&gSfxInstances[handle&255]);
            if(node==0) {
                node=NewCmd();
                node->kind=3;
                node->entry=&gSfxInstances[handle&255];
                node->value=-2.0f;
                node->stream=-2.0f;
                node->priority=-2.0f;
                node->route=value;
                SendCmd(node);
            } else node->route=value;
        }
    }
}
void AudioStopEffect(s32 handle) {
 Node *node;
 if(handle!=-1) {
  if(handle==gSfxInstances[handle&255].handle) {
   node=NewCmd();
   node->kind=2;
   node->entry=&gSfxInstances[handle&255];
   node->entry->flags[2]=1;node->entry->handle=0;
   SendCmd(node);
  }
 }
}
void AudioSetEffectFBPan(s32 handle,f32 value) {
    Node *node;
    if(handle!=-1) {
        if(handle==gSfxInstances[handle&255].handle) {
            node=FindChangeCmd(&gSfxInstances[handle&255]);
            if(node==0) {
                node=NewCmd();
                node->kind=3;
                node->entry=&gSfxInstances[handle&255];
                node->value=-2.0f;
                node->stream=-2.0f;
                node->route=-2.0f;
                node->priority=value;
                SendCmd(node);
            } else node->priority=value;
        }
    }
}
void AudioSetEffectLRPan(s32 handle,f32 value) {
    Node *node;
    if(handle!=-1) {
        if(handle==gSfxInstances[handle&255].handle) {
            node=FindChangeCmd(&gSfxInstances[handle&255]);
            if(node==0) {
                node=NewCmd();
                node->kind=3;
                node->entry=&gSfxInstances[handle&255];
                node->value=-2.0f;
                node->priority=-2.0f;
                node->route=-2.0f;
                node->stream=value;
                SendCmd(node);
            } else node->stream=value;
        }
    }
}
void AudioSetEffectVolume(s32 handle,f32 value) {
    Node *node;
    if(handle!=-1) {
        if(handle==gSfxInstances[handle&255].handle) {
            node=FindChangeCmd(&gSfxInstances[handle&255]);
            if(node==0) {
                node=NewCmd();
                node->kind=3;
                node->entry=&gSfxInstances[handle&255];
                node->stream=-2.0f;
                node->priority=-2.0f;
                node->route=-2.0f;
                node->value=value;
                SendCmd(node);
            } else node->value=value;
        }
    }
}
s32 AudioPlayEffect(s32 resource,f32 value,f32 stream)
{
 Entry *entry;
 Node *node;
 if(resource==-1)return -1;

 if(!GetSfxInstance(&entry))return PC_COUNTED(PC_SND_NO_ENTRY, -1);
 node=NewCmd();
 node->kind=0;
 node->entry=entry;
 entry->resource=resource;
 node->value=value;
 node->stream=stream;
 SendCmd(node);
 return entry->handle;
}
