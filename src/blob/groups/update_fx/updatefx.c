#include "game/game.h"

typedef struct FxTag {
    void *next, *prev;      /* 0x00 ll */
    void *list;             /* 0x08 */
    u32 id;                 /* 0x0C */
    s32 state;              /* 0x10 */
    s32 type;               /* 0x14 */
    s8 paused;              /* 0x18 */
    s8 loop;                /* 0x19 */
    u8 numCmds;             /* 0x1A */
    u8 priority;            /* 0x1B */
    s32 wpriority;          /* 0x1C */
    f32 startTime;          /* 0x20 */
    f32 vol;                /* 0x24 */
} FxTag;
typedef struct SortedTag {
    void *left, *right;     /* 0x00 tBSTE */
    FxTag *tag;             /* 0x08 */
} SortedTag;
typedef struct AudioMsg {
    s16 type;               /* 0x00 */
    s8 cmd;                 /* 0x02 */
    s8 used;                /* 0x03 */
    FxTag *tag;             /* 0x04 */
} AudioMsg;

/* PC: PC data, PC_SND_MSG_SLOTS long (pc_game_types.h, docs/port/DECOUPLE.md 9) */
#define gStopCmds (*(AudioMsg *(*)[PC_SND_MSG_SLOTS])pc_snd_stop_cmds)
#define gStartCmds (*(AudioMsg *(*)[PC_SND_MSG_SLOTS])pc_snd_start_cmds)
AudioMsg *GetCmd(void);      /* _GetCmd */

/* PC: a start or stop that finds the message pool empty (transform/group.c says when) waits for the next
 * priority pass. */
#define PC_CMD_OR_RETURN(c) if ((c) == 0) return;
/* PC: [audio] engines = all keeps the cars' engines out of the budget, and the counters see the sounds it holds back
 * that would play (one playing, or loud enough to start: port/config/voices.c, docs/port/AUDIO.md 10); at original it
 * is the same comparison. */
#define PC_OVER_BUDGET(tag, rank, budget) \
    pc_snd_over_budget((tag)->id, rank, budget, (tag)->state == 2 || (tag)->vol > 0.1f)
void UpdateFx(void *element, int elNum)
{
    SortedTag *stag = element;
    AudioMsg *cmd;
    { s32 t = elNum == 2; if (stag->tag->state != 2) { if (t) {} if (t) {} } }
    if (stag->tag->state != 1) {
        if (PC_OVER_BUDGET(stag->tag, elNum, gMaxSfx * 0.75f) ||
            (stag->tag->loop && stag->tag->state == 2 && stag->tag->vol <= 0.0f &&
             AudioTimeout(stag->tag->startTime, 0.2f))) {
            if (stag->tag->state == 2) {
                cmd = GetCmd(); PC_CMD_OR_RETURN(cmd)
                gStopCmds[gNumStopCmds++] = cmd;
                cmd->cmd = 5;
                cmd->tag = stag->tag;
                cmd->tag->numCmds++;
            }
        } else if (stag->tag->state != 2 &&
                   (stag->tag->vol > 0.1f || (!stag->tag->loop && stag->tag->type == 1))) {
            cmd = GetCmd(); PC_CMD_OR_RETURN(cmd)
            gStartCmds[gNumStartCmds++] = cmd;
            cmd->cmd = 3;
            cmd->tag = stag->tag;
            cmd->tag->numCmds++;
        }
    }
}
