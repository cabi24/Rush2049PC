#include "game/game.h"

void AudioSetMasterVolume(f32 value, u16 channel, s32 first, s32 second) {
    if (first) sound_fade_master_volume((u8)N64_F2U(value * 127.0f * music_volume_scale), channel, 1, 0);
    if (second) sound_fade_master_volume((u8)N64_F2U(value * 127.0f), channel, 0, 1);
}
