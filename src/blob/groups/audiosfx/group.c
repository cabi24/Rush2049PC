/* The sound server's requests (MAT3 audiosfx.c): an effect instance and the commands that start, change and stop
 * it, queued on gCmdQ for AudioSfxServer to carry out each retrace. The audio manager thread is the caller: these
 * are the effects the priority pass gives a voice. */
#include "game/game.h"
#include "game/audiomgr.h"

/* The queues in this file's view of the header's (gCmdQ without its volatile). */
VIEW(AudioList, gFreeQ);
#define gFreeQ gFreeQ_view
VIEW(AudioList, gCmdQ);
#define gCmdQ gCmdQ_view

tSfxCmd *FindChangeCmd(tSfxVoice *sound);
/* PC: an effect start that finds no free entry (AudioSfxInit's, gNumInstances of them) is counted (--log-voices,
 * port/config/voices.c, docs/port/AUDIO.md 10). */
#define PC_COUNTED(what, value) (pc_snd_count(what), value)

/* Puts cmd on list's head (InsertLLE before the head): the queues run from the tail, so this is their newest. */
void sfx_cmd_list_add(AudioList *list,tSfxCmd *cmd) {
 InsertLLE(list,cmd,list->head);
}

/* Takes the first unused effect instance (MAT3's _GetSfxInstance): clears its flags and voice and gives it a
 * handle of the next generation, (gNextIndex << 8) | its index. Returns 1 and the instance in *theSound, or 0 when
 * all maxSfx are in use. */
s32 GetSfxInstance(tSfxVoice **theSound)
{
 u32 i;
 *theSound=0;
 for(i=0;i<gNumInstances;i++) {
  if(gSfxInstances[i].used==0) {
   gSfxInstances[i].sound=-1;
   gSfxInstances[i].used=1;
   gSfxInstances[i].started=0;
   gSfxInstances[i].stop=0;
   gSfxInstances[i].stopped=0;
   gSfxInstances[i].stopAll=0;
   gSfxInstances[i].fail=0;
   gSfxInstances[i].index=(gNextIndex<<8)|i;
   if(gNextIndex>=0xFFFFFF)gNextIndex=1;
   else gNextIndex++;
   *theSound=&gSfxInstances[i];
   return 1;
  }
 }
 return 0;
}

/* Queues cmd (MAT3's _SendCmd). A stop of an instance that has not started yet is carried out at once instead:
 * its queued commands are dropped, the instance is freed and the stop goes back to the free queue. */
void SendCmd(tSfxCmd *cmd) {
 tSfxVoice *sound;tSfxCmd *cur,*next;
 if(cmd->cmd!=kSfxCmdMono && cmd->cmd==kSfxCmdStop && (sound=cmd->sound)->started==0) {
  if(cmd->cmd==kSfxCmdStop){}
  cur=gCmdQ.tail;
  while(cur) {
   next=cur->dlle.prev;
   if(sound==cur->sound) {
    RemoveLLE(&gCmdQ,cur);
    sfx_cmd_list_add(&gFreeQ,cur);
    sound=cmd->sound;
   }
   cur=next;
  }
  sound->used=0;sound->index=0;
  sfx_cmd_list_add(&gFreeQ,cmd);
 } else {
  sfx_cmd_list_add(&gCmdQ,cmd);
 }
}

/* Takes a command off the free queue's tail (MAT3's _NewCmd), its fail count cleared. Not checked for an empty
 * queue: there are three per instance (AudioSfxInit). */
tSfxCmd *NewCmd(void) {
 tSfxCmd *cmd=gFreeQ.tail;
 do{}while(0);
 cmd->failCounter=0;
 RemoveLLE(&gFreeQ,cmd);
 return cmd;
}

/* Queues the mono command: MusyX's output in mono (mono nonzero) or stereo. */
void AudioSetMono(s32 mono) {
 tSfxCmd *cmd=NewCmd();
 cmd->cmd=kSfxCmdMono;cmd->vol=(f32)mono;cmd->sound=0;
 SendCmd(cmd);
}

/* The four setters of a playing effect, each for the instance of handle theSound (a stale or -1 handle is
 * ignored): they add their value to the instance's queued change command, or queue one that changes only it. */

/* Sets the pitch (1: the sound's own). */
void AudioSetEffectPitch(s32 theSound,f32 pitch) {
    tSfxCmd *cmd;
    if(theSound!=-1) {
        if(theSound==gSfxInstances[theSound&255].index) {
            cmd=FindChangeCmd(&gSfxInstances[theSound&255]);
            if(cmd==0) {
                cmd=NewCmd();
                cmd->cmd=kSfxCmdChange;
                cmd->sound=&gSfxInstances[theSound&255];
                cmd->vol=-2.0f;
                cmd->lrpan=-2.0f;
                cmd->fbpan=-2.0f;
                cmd->pitch=pitch;
                SendCmd(cmd);
            } else cmd->pitch=pitch;
        }
    }
}
/* Queues the stop of the instance of handle theSound and invalidates the handle. */
void AudioStopEffect(s32 theSound) {
 tSfxCmd *cmd;
 if(theSound!=-1) {
  if(theSound==gSfxInstances[theSound&255].index) {
   cmd=NewCmd();
   cmd->cmd=kSfxCmdStop;
   cmd->sound=&gSfxInstances[theSound&255];
   cmd->sound->stop=1;cmd->sound->index=0;
   SendCmd(cmd);
  }
 }
}
/* Sets the front/back pan (-1 to 1). */
void AudioSetEffectFBPan(s32 theSound,f32 fbpan) {
    tSfxCmd *cmd;
    if(theSound!=-1) {
        if(theSound==gSfxInstances[theSound&255].index) {
            cmd=FindChangeCmd(&gSfxInstances[theSound&255]);
            if(cmd==0) {
                cmd=NewCmd();
                cmd->cmd=kSfxCmdChange;
                cmd->sound=&gSfxInstances[theSound&255];
                cmd->vol=-2.0f;
                cmd->lrpan=-2.0f;
                cmd->pitch=-2.0f;
                cmd->fbpan=fbpan;
                SendCmd(cmd);
            } else cmd->fbpan=fbpan;
        }
    }
}
/* Sets the left/right pan (-1 to 1). */
void AudioSetEffectLRPan(s32 theSound,f32 lrpan) {
    tSfxCmd *cmd;
    if(theSound!=-1) {
        if(theSound==gSfxInstances[theSound&255].index) {
            cmd=FindChangeCmd(&gSfxInstances[theSound&255]);
            if(cmd==0) {
                cmd=NewCmd();
                cmd->cmd=kSfxCmdChange;
                cmd->sound=&gSfxInstances[theSound&255];
                cmd->vol=-2.0f;
                cmd->fbpan=-2.0f;
                cmd->pitch=-2.0f;
                cmd->lrpan=lrpan;
                SendCmd(cmd);
            } else cmd->lrpan=lrpan;
        }
    }
}
/* Sets the volume (0 to 1). */
void AudioSetEffectVolume(s32 theSound,f32 vol) {
    tSfxCmd *cmd;
    if(theSound!=-1) {
        if(theSound==gSfxInstances[theSound&255].index) {
            cmd=FindChangeCmd(&gSfxInstances[theSound&255]);
            if(cmd==0) {
                cmd=NewCmd();
                cmd->cmd=kSfxCmdChange;
                cmd->sound=&gSfxInstances[theSound&255];
                cmd->lrpan=-2.0f;
                cmd->fbpan=-2.0f;
                cmd->pitch=-2.0f;
                cmd->vol=vol;
                SendCmd(cmd);
            } else cmd->vol=vol;
        }
    }
}
/* Starts effect soundId at volume and left/right pan on a new instance (the play command), and returns its
 * handle; -1 for soundId -1 or when every instance is in use. */
s32 AudioPlayEffect(s32 soundId,f32 volume,f32 pan)
{
 tSfxVoice *theSound;
 tSfxCmd *cmd;
 if(soundId==-1)return -1;

 if(!GetSfxInstance(&theSound))return PC_COUNTED(PC_SND_NO_ENTRY, -1);
 cmd=NewCmd();
 cmd->cmd=kSfxCmdPlay;
 cmd->sound=theSound;
 theSound->soundId=soundId;
 cmd->vol=volume;
 cmd->lrpan=pan;
 SendCmd(cmd);
 return theSound->index;
}
