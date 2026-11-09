/* AudioSetMasterVolume: the N64 form of MAT3's audiosysps2.c master-volume fade, on MusyX's volume groups. */
#include "game/game.h"

/* Fades the music (music nonzero) and/or the effects (sfx nonzero) to vol (0 to 1) over time milliseconds: the
 * audio manager thread's set-master-volume command. MusyX fades its volume groups 21 (songs) and 22 (effects)
 * (sound_fade_master_volume); the music's level is also scaled by music_volume_scale. The float-to-byte
 * conversions are IDO's (N64_F2U, pc_prelude.h). */
void AudioSetMasterVolume(f32 vol, u16 time, s32 music, s32 sfx) {
    if (music) sound_fade_master_volume((u8)N64_F2U(vol * 127.0f * music_volume_scale), time, 1, 0);
    if (sfx) sound_fade_master_volume((u8)N64_F2U(vol * 127.0f), time, 0, 1);
}
