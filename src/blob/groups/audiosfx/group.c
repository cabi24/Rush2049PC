#include "game/game.h"

typedef struct Entry {s8 flags[6];u8 pad6[2];s32 handle;s32 resource;u32 state16;s32 state20;} Entry;
typedef struct Node {struct Node *prev,*next;u8 kind,busy;u8 pad10[2];Entry *entry;f32 value,stream,priority,route;} Node;
typedef struct List {u32 pad0[2];Node *tail,*head;} List;
VIEW(List, gFreeQ);
#define gFreeQ gFreeQ_view
VIEW(List, gCmdQ);
#define gCmdQ gCmdQ_view

VIEW(u32, gNumInstances);
#define gNumInstances gNumInstances_view

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
