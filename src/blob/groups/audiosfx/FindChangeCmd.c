/* FindChangeCmd: the sound server's queued change command of an instance (MAT3 audiosfx.c's _FindChangeCmd). */
#include "game/game.h"
#include "game/audiomgr.h"

/* The command queue in this file's view of the header's: volatile, and its tail (MAT3's, at +0x10) a command. */
typedef struct CmdQueue { s32 words[3]; tSfxCmd *tail; } CmdQueue;
VIEW(volatile CmdQueue, gCmdQ);
#define gCmdQ gCmdQ_view

/* The change command queued for instance sound, or NULL: the setters (AudioSetEffectVolume ...) add to it rather
 * than queue another. Walks the queue from its tail (the oldest) through the links' prev, as AudioSfxServer runs
 * it. sound is passed as an integer, as this file always took it. */
tSfxCmd *FindChangeCmd(intptr_t sound) {
    tSfxCmd *cmd;
    for (cmd = (tSfxCmd *)gCmdQ.tail; cmd != 0; cmd = cmd->dlle.prev) {
        if ((intptr_t)cmd->sound == sound && cmd->cmd == kSfxCmdChange) return cmd;
    }
    return 0;
}
