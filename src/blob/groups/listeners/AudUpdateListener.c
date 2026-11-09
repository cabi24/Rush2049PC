/* AudUpdateListener: moves a listener (MAT3 audiomgr.c), and the empty listener check it calls. */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);
extern int __unit_dead[];

/* MAT3's _ValidateListener: a debug build's check of a listener, empty in the shipped game. The `if (0)` block
 * is not the game's: it keeps the function out of line for the PC build's unit merger
 * (src/blob/unit_overrides.json). */
void ValidateListener(tSfxListener *listener) {
    if (listener) {
    }
    if (0) {
        __unit_dead[0] = 1;
        __unit_dead[1] = 2;
        __unit_dead[2] = 3;
        __unit_dead[3] = 4;
        __unit_dead[4] = 5;
        __unit_dead[5] = 6;
        __unit_dead[6] = 7;
        __unit_dead[7] = 8;
    }
}

/* Moves listener (AudAddListener's) to pos, with velocity dir, forward axis look and up axis up; -1 (none) is
 * ignored. On the PC its links are two host pointers, so its vectors start at +0x14: with the N64's +0xC, each
 * frame's copy wrote the position over the links (docs/port/QA.md 5). */
void AudUpdateListener(tSfxListener *listener, f32 *pos, f32 *dir, f32 *look, f32 *up) {
    if (listener != (tSfxListener *) -1) {
        osRecvMesg(&audio_lock_queue, 0, 1);
        ValidateListener(listener);
        listener->pos[0] = pos[0];
        listener->pos[1] = pos[1];
        listener->pos[2] = pos[2];
        listener->dir[0] = dir[0];
        listener->dir[1] = dir[1];
        listener->dir[2] = dir[2];
        listener->look[0] = look[0];
        listener->look[1] = look[1];
        listener->look[2] = look[2];
        listener->up[0] = up[0];
        listener->up[1] = up[1];
        listener->up[2] = up[2];
        osSendMesg(&audio_lock_queue, 0, 0);
    }
}
