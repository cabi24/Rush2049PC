/* FindChangeCmd: the sound server's queued change command of an instance (MAT3 audiosfx.c's _FindChangeCmd). */
#include "game/game.h"
#include "game/audiomgr.h"


/* The change command queued for instance sound, or NULL: the setters (AudioSetEffectVolume ...) add to it rather
 * than queue another. Walks the queue from its tail (the oldest) through the links' prev, as AudioSfxServer runs
 * it. */
tSfxCmd *FindChangeCmd(tSfxVoice *sound) {
    tSfxCmd *cmd;
    for (cmd = (tSfxCmd *)gCmdQ.tail; cmd != 0; cmd = cmd->dlle.prev) {
        if (cmd->sound == sound && cmd->cmd == kSfxCmdChange) return cmd;
    }
    return 0;
}
