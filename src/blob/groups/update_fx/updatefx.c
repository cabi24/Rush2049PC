/* UpdateFx: the priority pass's decision for one fx tag (MAT3 audiomgr.c's _UpdateFx). */
#include "game/game.h"
#include "game/audiomgr.h"

/* PC: PC data, PC_SND_MSG_SLOTS long (pc_game_types.h, docs/port/DECOUPLE.md 9) */
#define gStopCmds (*(tAudioMsg *(*)[PC_SND_MSG_SLOTS])pc_snd_stop_cmds)
#define gStartCmds (*(tAudioMsg *(*)[PC_SND_MSG_SLOTS])pc_snd_start_cmds)

/* PC: a start or stop that finds the message pool empty (transform/group.c says when) waits for the next
 * priority pass. */
#define PC_CMD_OR_RETURN(c) if ((c) == 0) return;
/* PC: [audio] engines = all keeps the cars' engines out of the budget, and the counters see the sounds it holds back
 * that would play (one playing, or loud enough to start: port/config/voices.c, docs/port/AUDIO.md 10); at original it
 * is the same comparison. */
#define PC_OVER_BUDGET(tag, rank, budget) \
    pc_snd_over_budget((tag)->id, rank, budget, (tag)->state == 2 || (tag)->vol > 0.1f)

/* WalkBST's callback for each node of the tag tree, element (a tSortedFxTag) in rank elNum by weighted
 * priority. A tag ranked past three quarters of the effect quota (gMaxSfx), or a silent loop playing for more than
 * 0.2 s, loses its voice: an interrupt command if it plays. Otherwise a tag waiting for a voice that is audible
 * (above 0.1), or a one-shot emitter's, gets a start command. The commands are collected (gStopCmds,
 * gStartCmds) and run after the walk. Pending tags (just asked for) wait a pass. */
void UpdateFx(void *element, int elNum)
{
    tSortedFxTag *stag = element;
    tAudioMsg *cmd;
    /* A leftover of the N64 build's matching: evaluates elNum == 2 and the tag's state and does nothing. */
    { s32 t = elNum == 2; if (stag->tag->state != 2) { if (t) {} if (t) {} } }
    if (stag->tag->state != kFxPending) {
        if (PC_OVER_BUDGET(stag->tag, elNum, gMaxSfx * 0.75f) ||
            (stag->tag->loop && stag->tag->state == kFxPlaying && stag->tag->vol <= 0.0f &&
             AudioTimeout(stag->tag->startTime, 0.2f))) {
            if (stag->tag->state == kFxPlaying) {
                cmd = GetCmd(); PC_CMD_OR_RETURN(cmd)
                gStopCmds[gNumStopCmds++] = cmd;
                cmd->hdr.cmd = kAudioCmdInterruptEffect;
                cmd->intEffect.tag = stag->tag;
                cmd->intEffect.tag->numCmds++;
            }
        } else if (stag->tag->state != kFxPlaying &&
                   (stag->tag->vol > 0.1f || (!stag->tag->loop && stag->tag->type == kFxEmitter))) {
            cmd = GetCmd(); PC_CMD_OR_RETURN(cmd)
            gStartCmds[gNumStartCmds++] = cmd;
            cmd->hdr.cmd = kAudioCmdStartEffect;
            cmd->startEffect.tag = stag->tag;
            cmd->startEffect.tag->numCmds++;
        }
    }
}
